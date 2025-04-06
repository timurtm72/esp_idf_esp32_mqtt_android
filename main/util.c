#include "main.h"
uint8_t scale(float inValue,float inMinValue,float inMaxValue,float outMinValue,float outMaxValue){
return	 (uint8_t)  ((inValue - inMinValue) * 
   					(outMaxValue - outMinValue) / 
   					(inMaxValue - inMinValue) + outMinValue);
}


