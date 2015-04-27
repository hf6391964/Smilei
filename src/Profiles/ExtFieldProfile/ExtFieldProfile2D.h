#ifndef ExtFieldProfile2D_H
#define ExtFieldProfile2D_H


#include "ExtFieldProfile.h"
#include <cmath>

//  --------------------------------------------------------------------------------------------------------------------
//! 2D ExtField profile class
//  --------------------------------------------------------------------------------------------------------------------
class ExtFieldProfile2D : public ExtFieldProfile
{
    
public:
    ExtFieldProfile2D(ExtFieldStructure &extfield_struct);
    ~ExtFieldProfile2D() {};
    double operator() (std::vector<double>);
    
    
private:
    
    
};//END class

#endif
