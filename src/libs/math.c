#include "math.h"

int add(int a, int b)
{
    return a + b;
}

int sub(int a, int b)
{
    return a - b;
}

int mul(int a, int b)
{
    return a * b;
}

int div(int a, int b)
{
    return a / b;
}

int mod(int a, int b)
{
    return a % b;
}

int pow(int a, int b)
{
    int result = 1;
    for (int i = 0; i < b; i++)
    {
        result *= a;
    }
    return result;
}

int abs(int a)
{
    return a < 0 ? -a : a;
}

int max(int a, int b)
{
    return a > b ? a : b;
}

int min(int a, int b)
{
    return a < b ? a : b;
}

int clamp(int a, int min, int max)
{
    return a < min ? min : a > max ? max
                                   : a;
}

int sign(int a)
{
    return a < 0 ? -1 : a > 0 ? 1
                              : 0;
}

int sqrt(int a)
{
    int x = a;
    int y = 1;
    while (x > y)
    {
        x = (x + y) / 2;
        y = a / x;
    }
    return x;
}

int factorial(int a)
{
    int result = 1;
    for (int i = 1; i <= a; i++)
    {
        result *= i;
    }
    return result;
}
double fabs(double x) {
    return x < 0 ? -x : x;
}

double fmod(double x, double y) {
    int quotient = (int)(x / y);
    return x - y * quotient;
}
int is_prime(int a)
{
    if (a < 2)
    {
        return 0;
    }
    for (int i = 2; i <= sqrt(a); i++)
    {
        if (a % i == 0)
        {
            return 0;
        }
    }
    return 1;
}

int gcd(int a, int b)
{
    while (b != 0)
    {
        int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

int lcm(int a, int b)
{
    return a * b / gcd(a, b);
}

int is_even(int a)
{
    return a % 2 == 0;
}

int is_odd(int a)
{
    return a % 2 != 0;
}

int is_positive(int a)
{
    return a > 0;
}

int is_negative(int a)
{
    return a < 0;
}

int is_zero(int a)
{
    return a == 0;
}

int is_between(int a, int min, int max)
{
    return a >= min && a <= max;
}

int sum(int *a, int n)
{
    int result = 0;
    for (int i = 0; i < n; i++)
    {
        result += a[i];
    }
    return result;
}

static double normalize_angle(double x) {
    const double two_pi = 2 * PI;
    while (x > PI) x -= two_pi;
    while (x < -PI) x += two_pi;
    return x;
}

double sin(double x) {
    x = normalize_angle(x); // Reduce angle first
    double term = x;         // First term: x
    double result = term;
    double x_squared = x * x;

    for (int n = 1; n < 10; n++) {
        term *= -x_squared / ((2 * n) * (2 * n + 1)); // Update term
        result += term;
    }
    return result;
}

double cos(double x) {
    x = normalize_angle(x); // Reduce angle first
    double term = 1.0;       // First term: 1
    double result = term;
    double x_squared = x * x;

    for (int n = 1; n < 10; n++) {
        term *= -x_squared / ((2 * n - 1) * (2 * n)); // Update term
        result += term;
    }
    return result;
}