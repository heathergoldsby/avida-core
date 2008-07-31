#include "cUMLStateDiagram.h"
#include <set>

cUMLStateDiagram::cUMLStateDiagram()
{

  // initialize the state diagram with 10 states.
  sd0 = state_diagram(13); 

  // initialize / seed UML state diagram here
	orig = vertex(0, sd0);
	dest = vertex(0, sd0);
  
  trans_label_index = 0;
  trigger_index = 0;
  guard_index = 0;
  action_index = 0;
  looping = 0;

  xmi = "";
    
}

cUMLStateDiagram::~cUMLStateDiagram()
{
}

// Technically, this does not actually return the number of non-deterministic states, but rather
// returns >0 if it is non-deterministic and 0 if it is deterministic.
unsigned int cUMLStateDiagram::getNumberOfNonDeterministicStates() {
	std::string tt, tg, ta, ts;
	unsigned int numNonD=0;
	boost::graph_traits<state_diagram>::out_edge_iterator ei, ei_end;
	boost::graph_traits<state_diagram>::vertex_iterator vi, vi_end;

	// For each...
	for(tie(vi,vi_end) = vertices(sd0); vi != vi_end; ++vi) {
		std::set<std::string> tnames;
		// out-edge of each vertex:
		for(tie(ei,ei_end) = out_edges(*vi, sd0); ei != ei_end; ++ei) {
			boost::graph_traits<state_diagram>::edge_descriptor e = *ei;
			
			// Build a string from the trigger and guard for this out-edge.
			tt = triggers[(sd0[e]._tr)].operation_id;
			tg = guards[(sd0[e]._gu)];
			if (tt == "<null>") tt = "";
			if (tg == "<null>") tg = "";
			ts = tt + "[" + tg + "]";
			
			// See if it's already in the set:
			if(tnames.find(ts) == tnames.end()) {
				// no; add it.
				tnames.insert(ts);
			} else {
				// yes; increment the count of non-deterministic states.
				// We've found a state that has out-edges where the trigger+guard
				// combination is duplicated at least once.
				++numNonD;
				break;
			}
			
		}
		
		// if there is an empty trigger / guard pair AND there are multiple
		// transitions leaving this state, then increment the number of non-deterministic
		// states
		ts = "[]";
		if ((tnames.find(ts) != tnames.end()) && (tnames.size() > 1)) ++numNonD;
		
	}
	return numNonD;
}


// This function accepts a path throught the state diagram as a deque and returns the length 
// of the longest path segment that the diagram satisfies. 
// The function is complicated by the fact that the longest path segment could start at the 
// beginning, middle, or end of the path itself. 
int cUMLStateDiagram::findPath(std::deque<std::string> p, bool should_loop, int start_state) { 
	unsigned int path_dist = 0; // the current path distance satisfied is 0. 
//	int path_longest_length = 0; 
	unsigned int len = 0;
	int num_vert = num_vertices(sd0);
	std::deque<std::string> p_temp = p;
	int actual_path_start;
	int bonus = 0;
			
		while (!p.empty()) {
			for (int i = 0; i<num_vert; i++) { 
				len = checkForPathStep(p, vertex(i, sd0), 0);
				// check to see if this path is longer than other paths....
				if (len > path_dist) { 
					actual_path_start = i;
					path_dist = len;
				}
			}	
			p.pop_front(); 
		
			if (len > p.size()) break;
		} 
	
	bonus = path_dist;
	if (start_state != -1) { 
		if (start_state == actual_path_start) { 
			bonus += 1;
		}
	}
	if (should_loop == 1 && (path_dist == p_temp.size())) { 
		getEndState(p_temp, actual_path_start);		
		if (actual_end_state == actual_path_start) bonus +=1;
	}

	return bonus;
}


int cUMLStateDiagram::checkForPathStep(std::deque<std::string> path, 
				boost::graph_traits<state_diagram>::vertex_descriptor v, 
				int curr_dist) { 
	
	boost::graph_traits<state_diagram>::out_edge_iterator out_edge_start, out_edge_end;
	boost::graph_traits<state_diagram>::edge_descriptor ed;
	std::string tt, tg, ta, ts;
	int longest_dist = curr_dist;
	int temp;

	if (path.empty()) {
		return curr_dist;
	}	

	// Get the outgoing edges of v
	for(tie(out_edge_start, out_edge_end) = out_edges(v, sd0);
			 out_edge_start != out_edge_end; ++out_edge_start) { 
			ed = *out_edge_start;
			
			// Get the label of the edge
			tt = triggers[(sd0[ed]._tr)].operation_id;
			tg = guards[(sd0[ed]._gu)];
			ta = actions[(sd0[ed]._act)];
			
			// eliminate nulls
			if (tt == "<null>") tt = "";
			if (tg == "<null>") tg = "";
			if (ta == "<null>") ta = "";
			
			ts = tt + "[" + tg + "]" + "/" + ta;
//			std::cout << "transition named: " << ts << std::endl;
			
			std::string blah = path.front();
			
			if (ts == path.front()) { 

				temp = checkForPathStep(std::deque<std::string>(++path.begin(), path.end()), target(ed,sd0), curr_dist+1);
				
				if (temp > longest_dist) {
					longest_dist = temp;
				}	
			} 
			
	}
	return longest_dist;

} 

// Given a path and a start state, check if the end state is the same as the start state.
int cUMLStateDiagram::getEndState(std::deque<std::string> path, 
		boost::graph_traits<state_diagram>::vertex_descriptor v) {
	
	boost::graph_traits<state_diagram>::out_edge_iterator out_edge_start, out_edge_end;
	boost::graph_traits<state_diagram>::edge_descriptor ed;
	std::string tt, tg, ta, ts;
	int temp;
	
	if (path.empty()) {
		actual_end_state = v;
		return v;
	}	

	// Get the outgoing edges of v
	for(tie(out_edge_start, out_edge_end) = out_edges(v, sd0);
			 out_edge_start != out_edge_end; ++out_edge_start) { 
			ed = *out_edge_start;
			
			// Get the label of the edge
			tt = triggers[(sd0[ed]._tr)].operation_id;
			tg = guards[(sd0[ed]._gu)];
			ta = actions[(sd0[ed]._act)];
			
			// eliminate nulls
			if (tt == "<null>") tt = "";
			if (tg == "<null>") tg = "";
			if (ta == "<null>") ta = "";
			
			ts = tt + "[" + tg + "]" + "/" + ta;
						
			if (ts == path.front()) { 
				temp = getEndState(std::deque<std::string>(++path.begin(), path.end()), target(ed,sd0));
			} 
	}
	
	return v;

}




bool cUMLStateDiagram::findTrans(int origin, int destination, int trig, int gu, int act) 
{

	bool result = false;
	int num_vert = (int) num_vertices(sd0); 
	
	if (num_edges(sd0) == 0) { 
		return false;
	}
	
	boost::graph_traits<state_diagram>::vertex_descriptor o_temp, d_temp;
	boost::graph_traits<state_diagram>::edge_iterator edge_start, edge_end;
	boost::graph_traits<state_diagram>::out_edge_iterator out_edge_start, out_edge_end;
	boost::graph_traits<state_diagram>::in_edge_iterator in_edge_start, in_edge_end;
	boost::graph_traits<state_diagram>::edge_descriptor ed;

	// Get the set of transitions.
	if ((origin == -1) && (destination == -1)) { 
		for (tie(edge_start, edge_end) = edges(sd0);
			 edge_start != edge_end; ++edge_start) { 
			ed = *edge_start;
			if (((trig == -1)	|| (sd0[ed]._tr == trig))		&& 
				((gu == -1)		|| (sd0[ed]._gu == gu))			&& 
				((act == -1)	|| (sd0[ed]._act == act)))  { 
				result = true; 
				break;
			}	
		}
	} else if (origin == -1 && destination != -1) { 
		if (destination > num_vert) return result;
		d_temp = vertex(destination, sd0);
		for (tie(in_edge_start, in_edge_end) = in_edges(d_temp, sd0);
			 in_edge_start != in_edge_end; ++in_edge_start) { 
			ed = *in_edge_start;
			if (((trig == -1)	|| (sd0[ed]._tr == trig))		&& 
				((gu == -1)		|| (sd0[ed]._gu == gu))			&& 
				((act == -1)	|| (sd0[ed]._act == act)))  { 
				result = true; 
				break;
			}	
		}
	} else if (origin != -1 && destination == -1) { 
		if (origin > num_vert) return result;
		o_temp = vertex(origin, sd0);

		for(tie(out_edge_start, out_edge_end) = out_edges(o_temp, sd0);
			 out_edge_start != out_edge_end; ++out_edge_start) { 
			ed = *out_edge_start;
			if (((trig == -1)	|| (sd0[ed]._tr == trig))		&& 
				((gu == -1)		|| (sd0[ed]._gu == gu))			&& 
				((act == -1)	|| (sd0[ed]._act == act)))  { 
				result = true; 
				break;
			}	
		}
	} else if (origin != -1 && destination != -1) { 
		if ((destination > num_vert) || 
			(origin > num_vert)) return result;
		o_temp = vertex(origin, sd0);
		d_temp = vertex(destination, sd0);
		for(tie(out_edge_start, out_edge_end) = edge_range (o_temp, d_temp, sd0);
			 out_edge_start != out_edge_end; ++out_edge_start) { 
			ed = *out_edge_start;
			if (((trig == -1)	|| (sd0[ed]._tr == trig))		&& 
				((gu == -1)		|| (sd0[ed]._gu == gu))			&& 
				((act == -1)	|| (sd0[ed]._act == act)))  { 
				result = true; 
				break;
			}	
		}
	}
	
	return result;

}



template <typename T>
bool cUMLStateDiagram::absoluteMoveIndex (T x, int &index, int amount )
{

	int x_size = (int) x.size();
	if (x_size == 0 || amount > x_size) {
		return false;
	}
	
	index = 0;
	return relativeMoveIndex(x, index, amount);
}

template <typename T>
bool cUMLStateDiagram::relativeMoveIndex (T x, int &index, int amount )
{
	int x_size = (int) x.size();

	if (x_size == 0) {
		return false;
	}
	
	if (amount > 0) { 
		index += (amount % x_size);

		// index is greater than vector
		if (index >= x_size) { 
			index -= x_size;
		} else if(index < 0) { 
			index += x_size;
		}
	}	
		
	return true;
}



bool cUMLStateDiagram::absoluteJumpTrigger(int jump_amount)
{
	return absoluteMoveIndex(triggers, trigger_index, jump_amount);
}

bool cUMLStateDiagram::absoluteJumpGuard(int jump_amount)
{
	return absoluteMoveIndex(guards, guard_index, jump_amount);	
}

bool cUMLStateDiagram::absoluteJumpAction(int jump_amount)
{
	return absoluteMoveIndex(actions, action_index, jump_amount);
}

bool cUMLStateDiagram::absoluteJumpTransitionLabel(int jump_amount)
{
	return absoluteMoveIndex(transition_labels, trans_label_index, jump_amount);
}

bool cUMLStateDiagram::absoluteJumpOriginState(int jump_amount) 
{	
	bool result = false;
	int num_vert = (int) num_vertices(sd0);
	
	if (num_vert > jump_amount) {  	
		orig = vertex(jump_amount, sd0);
		result = true;
	}
	return result;
}

bool cUMLStateDiagram::absoluteJumpDestinationState(int jump_amount) 
{	
	bool result = false;
	int num_vert = (int) num_vertices(sd0);

	if (num_vert > jump_amount) {  	
		dest = vertex(jump_amount, sd0);
		result = true;
	}
	return result;
}


bool cUMLStateDiagram::relativeJumpTrigger(int jump_amount)
{
	return relativeMoveIndex(triggers, trigger_index, jump_amount);
}

bool cUMLStateDiagram::relativeJumpGuard(int jump_amount)
{
	return relativeMoveIndex(guards, guard_index, jump_amount);	
}

bool cUMLStateDiagram::relativeJumpAction(int jump_amount)
{
	return relativeMoveIndex(actions, action_index, jump_amount);
}

bool cUMLStateDiagram::relativeJumpTransitionLabel(int jump_amount)
{
	int size = transition_labels.size();
	return relativeMoveIndex(transition_labels, trans_label_index, jump_amount);
}

bool cUMLStateDiagram::relativeJumpOriginState(int jump_amount) 
{	
	bool result = true;
	unsigned int num_vert = (int) num_vertices(sd0);
	
	if (jump_amount > 0) { 
		orig += (jump_amount % num_vert);
		
		// index is greater than vector
		if (orig >= num_vert) { 
			orig -= num_vert;
		} 
	}	
	
	
	return result;
}

bool cUMLStateDiagram::relativeJumpDestinationState(int jump_amount) 
{	
	bool result = true;
	unsigned int num_vert = (int) num_vertices(sd0);
	
	if (jump_amount > 0) { 
		dest += (jump_amount % num_vert);
		
		// index is greater than vector
		if (dest >= num_vert) { 
			dest -= num_vert;
		} 
	}	
	return result;
}

bool cUMLStateDiagram::addTrigger(std::string op_id, std::string lab) 
{
	trigger_info t;
	t.operation_id = op_id;
	t.label = lab;
	
	triggers.push_back(t);
	
	return true;
}

bool cUMLStateDiagram::addGuard(std::string gu) 
{
	guards.push_back(gu);
	return true;
}

bool cUMLStateDiagram::addAction(std::string act)
{
	actions.push_back(act);
	return true;
}

bool cUMLStateDiagram::addTransitionLabel(int t, int g, int a) { 
	
	transition_properties tp = transition_properties(t, g, a); 
	transition_labels.push_back(tp); 
	return true;
}


bool cUMLStateDiagram::addTransitionTotal(int o, int d, int t, int g, int a) { 
	absoluteJumpGuard(g);
	absoluteJumpAction(a);
	absoluteJumpTrigger(t);
	absoluteJumpOriginState(o);
	absoluteJumpDestinationState(d);	
	return addTransitionTotal();

}

bool cUMLStateDiagram::addTransitionFromLabel() {

	bool val = true; 
	
	if (looping) { 
//		std::cout << "Adding loop element " << trans_label_index << std::endl; 
		
		loop_trans_labels.push_back(trans_label_index);
	} else{
		// get the properties of the transition label. 
		transition_properties tp = transition_labels[trans_label_index];
	
		absoluteJumpTrigger(tp._tr);
		absoluteJumpGuard(tp._gu); 
		absoluteJumpAction(tp._act);
	
		val = addTransitionTotal();
	} 
	return val;
}

bool cUMLStateDiagram::addTransitionTotal()
{
	// Check that there are two vertices
	if (num_vertices(sd0) == 0) {
		return false;
	} 
		
	// Disable transitions from one state to itself. 
	
	if (orig == dest) return false;

	// Check that there is not a duplicate transition
	if (findTrans(orig, dest, trigger_index, guard_index, action_index)) { 
		return false;
	}
	
	// Create the transition properties
	transition_properties tp = transition_properties(trigger_index, guard_index, action_index); 
	
	// Create the transition
	add_edge(orig, dest, tp, sd0);
		
	// Reset all the indices
//	trigger_index = 0;
//	action_index = 0;
//	guard_index = 0;
//	orig = vertex(0, sd0);
//	dest = vertex(0, sd0);
		
	return true;

}


int cUMLStateDiagram::numStates()
{
	return num_vertices(sd0);
}

int cUMLStateDiagram::numTrans()
{
	return num_edges(sd0);
} 

// print the label. Change - signs to _
std::string cUMLStateDiagram::StringifyAnInt(int x) { 

	std::ostringstream o;

	if (x < 0) {
		x = abs(x);
		o << "_";
	} 
	
	o << x;
	return o.str();
}

std::string cUMLStateDiagram::getXMI(std::string name)
{
	printXMI(name);
	return (xmi);
}

void cUMLStateDiagram::printXMI(std::string name)
{
	std::string temp, temp1, temp2, temp3;
	std::string trig_label, trig_op_name;
	std::string sd = name;
	

	boost::graph_traits<state_diagram>::edge_iterator edge_start, edge_end;
	boost::graph_traits<state_diagram>::edge_descriptor ed;

	int s_count = 0;
	int t_count = 0;
	xmi = "";

	// This state is the initial state; thus it should be printed regardless of whether it has an incoming
	// edge or not.
	if (numStates() > 0) {
		temp = "_1";
		xmi += "<UML:Pseudostate xmi.id=\"" + sd + "s"  + temp + "\" kind=\"initial\" outgoing=\"\" name=\"s";
		xmi += temp + "\" isSpecification=\"false\"/>\n";
	}
	
	for (; s_count < numStates(); ++s_count) {
				temp = sd + "s" + StringifyAnInt(s_count);
			xmi+="<UML:CompositeState xmi.id=\"";
			xmi+=temp;
			xmi+= "\" isConcurrent=\"false\" name=\""; 
			xmi+= temp; 
			xmi+= "\" isSpecification=\"false\"/>\n";
	}
		
		// end the set of states....
		xmi+= "</UML:CompositeState.subvertex>\n";
		xmi+= "</UML:CompositeState>\n";
		xmi+= "</UML:StateMachine.top>\n";
		
		// start the set of transitions...
		xmi+="<UML:StateMachine.transitions>\n";

	// Get the set of transitions.
	for (tie(edge_start, edge_end) = edges(sd0);
			 edge_start != edge_end; ++edge_start) {
		
		ed = *edge_start;
	 	 
		// info determined from the trans itself....
		temp = sd + "t" + StringifyAnInt(t_count);
		temp1 = sd + "s" + StringifyAnInt(source(ed, sd0));
		temp2 = sd + "s" + StringifyAnInt(target(ed, sd0));
		temp3 = temp + temp1 + temp2;
		
		xmi+= "<UML:Transition xmi.id=\"" + temp3 + "\"";
		xmi+= " source=\"" + temp1 + "\"";
		xmi += " target=\"" + temp2 + "\" name=\"\" isSpecification=\"false\">\n";

		// Get guard, trigger, and action
		temp1 = guards[(sd0[ed]._gu)];
		temp2 = actions[(sd0[ed]._act)];
		trig_label = triggers[(sd0[ed]._tr)].label;
		trig_op_name = triggers[(sd0[ed]._tr)].operation_id;

		// print trigger, if any
		if (trig_label != "<null>") {
			xmi+= "<UML:Transition.trigger> <UML:Event> <UML:ModelElement.namespace> <UML:Namespace> ";
			xmi+= "<UML:Namespace.ownedElement> <UML:CallEvent xmi.id=\"" + temp3;
			xmi+= "tt\"  operation=\""+ trig_label + "\" ";
			xmi+= "name=\"" + trig_op_name + "\" isSpecification=\"false\"/> "; 
			xmi+= "</UML:Namespace.ownedElement> </UML:Namespace> </UML:ModelElement.namespace> ";
			xmi+= "</UML:Event>  </UML:Transition.trigger> ";
		}
		
		// print guard, if any
		// Note: for guard to work, '<' => '&lt'
		if (temp1 != "<null>"){
			xmi+= "<UML:Transition.guard> <UML:Guard> <UML:Guard.expression> ";
			xmi+= "<UML:BooleanExpression body=\"" + temp1 + "\" language=\"\"/> ";
			xmi+= "</UML:Guard.expression> </UML:Guard> </UML:Transition.guard> ";
		}
		
		// print action, if any
		if (temp2 != "<null>") { 
			xmi+= "<UML:Transition.effect> <UML:UninterpretedAction xmi.id=\"" + temp3 + "ui\" ";
			xmi+= "isAsynchronous=\"false\" name=\"\" isSpecification=\"false\"> ";
			xmi+= "<UML:Action.script> <UML:ActionExpression language=\"\" body=\""; 
			xmi+= temp2 + "\"/> </UML:Action.script> </UML:UninterpretedAction> </UML:Transition.effect> ";		
		}
		
		xmi += "</UML:Transition>\n";
		t_count++;
	}
	
	// Add one transition to connect the init state to state 0
	temp = sd + "t" + StringifyAnInt(t_count);
	temp1 = sd + "s" + StringifyAnInt(-1);
	temp2 = sd + "s" + StringifyAnInt(0);
	temp3 = temp + temp1 + temp2;

	xmi+= "<UML:Transition xmi.id=\"" + temp3 + "\"";
	xmi+= " source=\"" + temp1 + "\"";
	xmi += " target=\"" + temp2 + "\" name=\"\" isSpecification=\"false\">\n";
	xmi += "</UML:Transition>\n";
	xmi +=  "</UML:StateMachine.transitions> </UML:StateMachine> </UML:Namespace.ownedElement> ";
    xmi +=  " </UML:Class>";
		
	return;
}


void cUMLStateDiagram::printStateDiagram()
{
	std::string temp, temp1, temp2, temp3;
	std::string trig_label, trig_op_name;
	

	boost::graph_traits<state_diagram>::edge_iterator edge_start, edge_end;
	boost::graph_traits<state_diagram>::edge_descriptor ed;

	int s_count = 0;
	// This state is the initial state; thus it should be printed regardless of whether it has an incoming
	// edge or not.

	std::cout << "States: \n\n";
	for (; s_count < numStates(); ++s_count) {
				temp =  "s" + StringifyAnInt(s_count);
			std::cout << "State: ";
			std::cout << temp << std::endl;
	}
		

	std::vector<trigger_info>::iterator trig_it;
	int count = 0;
	for (trig_it = triggers.begin(); trig_it < triggers.end(); trig_it++) { 
		std::cout << "trigger " << count << " " << (*trig_it).operation_id << std::endl;
		count++;
	}

	std::vector<std::string>::iterator act_it;
	count = 0;
	for (act_it = guards.begin(); act_it < guards.end(); act_it++) { 
		std::cout << "guard " << count << " " << (*act_it) << std::endl;
		count++;
	}
		
	count = 0;
	for (act_it = actions.begin(); act_it < actions.end(); act_it++) { 
		std::cout << "action " << count << " " << (*act_it) << std::endl;
		count++;
	}	
	
	count = 0; 
	std::vector<transition_properties>::iterator tl_it;
	for (tl_it = transition_labels.begin(); tl_it < transition_labels.end(); tl_it++) { 
		std::cout << triggers[(*tl_it)._tr].operation_id << " " << guards[(*tl_it)._gu];
		std::cout << " " << actions[(*tl_it)._act] << std::endl;
	}
					
		
	return;
}


// hjg: error in here. Last element not adding correctly?
void cUMLStateDiagram::endLooping() { 
	looping = false; 
	
	// set up the origin
	absoluteJumpOriginState(loop_state);
	
	
	// for each element of the loop... 
	while (loop_trans_labels.size()!=0){
		// set up the destination
		if(loop_trans_labels.size() == 1) { 
			// last transition loop to start
			absoluteJumpDestinationState(loop_state);
		} else { 
			relativeJumpDestinationState(1);
		}
		
		// set up the label
		absoluteJumpTransitionLabel(loop_trans_labels.front()); 
		int loop_int = loop_trans_labels.front();
		addTransitionFromLabel();
//		std::cout << "deque front " << loop_trans_labels.front() << " " << orig << " " << dest << std::endl;
		loop_trans_labels.pop_front();
		relativeJumpOriginState(1);

	}

}


