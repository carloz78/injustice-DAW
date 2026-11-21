#pragma once
// Compás actual del proyecto (numerador/denominador). Los pulsos de Tracktion son negras: pulsos por compás = num * 4 / den.
namespace timesig
{
    inline int num = 4, den = 4;
    inline double bar() { return num * 4.0 / den; }
}
