/*
 *   (c) Yury Kuchura
 *   kuchura@gmail.com
 *
 *   This code can be used on terms of WTFPL Version 2 (http://www.wtfpl.net/).
 */

#ifndef _MATCH_H_
#define _MATCH_H_

#include <stdint.h>
#include <complex.h>

/**
    @brief   Calculate L-Networks on ideal lumped elements for given load impedance
    @param   ZL Load impedance
*/
void MATCH_Calc(int X0, int Y0, float complex ZL);


#endif // _MATCH_H_
