/**
 * @file
 * @brief Linkdef for ROOT Class generation
 */

#pragma link off all globals;
#pragma link off all classes;
#pragma link off all functions;

// AP2 objects
#pragma link C++ class corryvreckan::TestBeamObject + ;
#pragma link C++ class corryvreckan::Pixel + ;

// Vector of Object for internal storage
//#pragma link C++ class std::vector < allpix::Object* > +;
