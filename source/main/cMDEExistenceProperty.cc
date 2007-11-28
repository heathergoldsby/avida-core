/*
 *  cMDEExistenceProperty.cc
 *  
 *
 *  Created by Heather Goldsby on 11/20/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#include "cMDEExistenceProperty.h"

void cMDEExistenceProperty::print() {
	
	std::ofstream outfile;
	outfile.open (_name.c_str());
	assert(outfile.is_open());
	
	outfile << "#define p (" << _expr_p << ")" << std::endl;
	outfile << "never { /* !(<>(p)) */ " << std::endl;
	outfile << "accept_init :    /* init */" << std::endl;
	outfile << "if " << std::endl;
	outfile << ":: (!p) -> goto accept_init " << std::endl;
	outfile << "fi; }" << std::endl;
	
	outfile.close();

}

void cMDEExistenceProperty::printWitness() {
	
	std::ofstream outfile;
	std::string file_name = "w" + _name;
	outfile.open (file_name.c_str());
	assert(outfile.is_open());
	
	outfile << "#define p (" << _expr_p << ")" << std::endl;
	outfile << "never { /* !([](!p)) */ " << std::endl;
	outfile << "T0_init :    /* init */ " << std::endl;
	outfile << "if " << std::endl;
	outfile << ":: (1) -> goto T0_init" << std::endl;
	outfile << ":: (p) -> goto accept_all " << std::endl;
	outfile << "fi; " << std::endl;
	outfile << "accept_all :    /* 1 */" << std::endl;
	outfile << "skip }" << std::endl;

	
	outfile.close();
	
}
