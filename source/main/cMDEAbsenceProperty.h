/*
 *  cMDEAbsenceProperty.h
 *  
 *
 *  Created by Heather Goldsby on 11/20/07.
 *  Copyright 2007 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _C_MDEABSENCEPROPERTY_H_
#define _C_MDEABSENCEPROPERTY_H_

#include "cMDEProperty.h"
#include <string>
#include <fstream>
#include <iostream>


/* Used to represent properties of the form: Globally, it is never the case that P holds  */

class cMDEAbsenceProperty : public cMDEProperty{
	
public:
	cMDEAbsenceProperty(std::string expr, std::string q) { _expr_p = expr; 
		_name = ("Absence" + q); _reward = -1; }
	virtual ~cMDEAbsenceProperty() {}
	void print(); // { std::cout << _scope << " " << _expr_p  << std::endl; }
	void printWitness(); // { std::cout << _scope << " " << _expr_p  << std::endl; }
	void evaluate();

private:
	std::string _expr_p;
};

#endif