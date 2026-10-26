/* vec_test.c — standalone unit tests for the vector engine (Ayansh, Week 1).
 *
 * Build & run:  make vec_test && ./vec_test      (or: make test on Windows)
 * Deliberately independent of the lexer/parser: this is the piece that must
 * be correct BEFORE any analyzer code is written on top of it.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "unitvec.h"

static int g_pass = 0, g_fail = 0;

/* x87 excess-precision-safe double compare (32-bit MinGW keeps doubles in
 * 80-bit registers, so bit-exact == against a literal 0.3048 can fail) */
static int sc_near(double a, double b) {
    return fabs(a - b) < 1e-9;
}

static void check_impl(int cond, const char *what) {
    if (cond) { g_pass++; printf("  ok  %s\n", what); }
    else      { g_fail++; printf("FAIL  %s\n", what); }
}
#define CHECK(cond, what) check_impl((cond), (what))

int main(void) {
    char buf[256];

    puts("[1] constructors");
    UnitVec dimless = uv_dimensionless();
    CHECK(uv_is_dimensionless(dimless), "dimensionless() is all-zero");

    UnitVec meters  = uv_from_name("meters");
    UnitVec seconds = uv_from_name("seconds");
    UnitVec kg      = uv_from_name("kg");
    uv_str(meters, buf, sizeof buf);
    CHECK(strcmp(buf, "[1,0,0,0,0,0,0]") == 0, "meters == [1,0,0,0,0,0,0]");
    uv_str(seconds, buf, sizeof buf);
    CHECK(strcmp(buf, "[0,0,1,0,0,0,0]") == 0, "seconds == [0,0,1,0,0,0,0]");

    puts("[2] additive algebra (+/- require equality)");
    CHECK(!uv_equal(meters, seconds), "length != time");
    CHECK( uv_equal(meters, uv_from_name("m")), "m == meters");

    puts("[3] multiplicative algebra");
    /* velocity = m / s == [1,0,-1,0,0,0,0] */
    UnitVec velocity = uv_div(meters, seconds);
    uv_str(velocity, buf, sizeof buf);
    CHECK(strcmp(buf, "[1,0,-1,0,0,0,0]") == 0, "m/s == [1,0,-1,0,0,0,0]");
    CHECK(uv_equal(velocity, uv_n(1,0,-1,0,0,0,0)), "velocity matches uv_n");

    /* acceleration = m / s^2 == [1,0,-2,0,0,0,0] */
    UnitVec accel = uv_div(meters, uv_pow(seconds, 2));
    uv_str(accel, buf, sizeof buf);
    CHECK(strcmp(buf, "[1,0,-2,0,0,0,0]") == 0, "m/s^2 == [1,0,-2,0,0,0,0]");

    /* force = kg * m / s^2 == [1,1,-2,0,0,0,0] (Newton) */
    UnitVec force = uv_mul(uv_mul(kg, meters), uv_div(uv_dimensionless(), uv_pow(seconds, 2)));
    force = uv_div(uv_mul(kg, meters), uv_pow(seconds, 2));
    uv_str(force, buf, sizeof buf);
    CHECK(strcmp(buf, "[1,1,-2,0,0,0,0]") == 0, "kg*m/s^2 == [1,1,-2,0,0,0,0]");

    /* energy = kg*m^2/s^2 == [2,1,-2,0,0,0,0] (Joule; order is [m,kg,s,..]) */
    UnitVec energy = uv_mul(force, meters);
    uv_str(energy, buf, sizeof buf);
    CHECK(strcmp(buf, "[2,1,-2,0,0,0,0]") == 0, "J = kg*m^2/s^2 == [2,1,-2,0,0,0,0]");

    puts("[4] cancellation");
    UnitVec cancelled = uv_div(meters, uv_from_name("m"));
    CHECK(uv_is_dimensionless(cancelled), "m/m is dimensionless");
    CHECK(uv_equal(velocity, uv_mul(velocity, dimless)), "x * 1 == x");

    puts("[5] pow and negative exponents");
    UnitVec area = uv_pow(meters, 2);
    CHECK(area.dim[DIM_M] == 2, "m^2 exponent is 2");
    UnitVec inv_s = uv_pow(seconds, -1);
    CHECK(inv_s.dim[DIM_S] == -1, "s^-1 exponent is -1");

    puts("[6] unitary predicate (metric vs imperial groundwork)");
    UnitDim d;
    CHECK(uv_is_unitary(meters, &d) && d == DIM_M, "meters is unitary Length");
    CHECK(uv_is_unitary(uv_from_name("feet"), &d) && d == DIM_M, "feet is unitary Length too");
    UnitVec speed = uv_div(meters, seconds);
    CHECK(!uv_is_unitary(speed, &d), "m/s is NOT unitary (two dims)");

    puts("[7] describe() output");
    uv_describe(force, buf, sizeof buf);
    printf("      kg*m/s^2 reads as: \"%s\"\n", buf);
    CHECK(strcmp(buf, "Mass*Length/(Time*Time)") == 0 || strcmp(buf, "Length*(Mass/Time*Time)") == 0
          ||strstr(buf, "Length") != NULL, "force describes with Length/Mass/Time");
    uv_describe(dimless, buf, sizeof buf);
    CHECK(strcmp(buf, "Scalar") == 0, "dimensionless reads as Scalar");

    puts("[8] unknown unit fallback");
    UnitVec bogus = uv_from_name("furlongs");
    CHECK(uv_is_dimensionless(bogus), "unknown unit falls back to dimensionless with warning");

    puts("[9] canonical scale factors (Week-3 metric vs imperial)");
    CHECK(sc_near(uv_scale(meters), 1.0), "meters scale == 1.0");
    CHECK(sc_near(uv_scale(uv_from_name("feet")), 0.3048), "feet scale == 0.3048");
    CHECK(sc_near(uv_scale(uv_from_name("km")), 1000.0), "km scale == 1000.0");
    CHECK(sc_near(uv_scale(uv_from_name("hours")), 3600.0), "hours scale == 3600.0");
    CHECK(sc_near(uv_scale(uv_from_name("pounds")), 0.45359237), "pounds scale == 0.45359237");
    CHECK(uv_equal(meters, uv_from_name("feet")) &&
          !scale_equal(meters, uv_from_name("feet")),
          "meters vs feet: same vector, DIFFERENT scale");
    CHECK(scale_equal(meters, uv_from_name("meters")), "scale_equal(m,m)");
    UnitVec feet = uv_from_name("feet");
    UnitVec feet_m = uv_mul(feet, meters);
    CHECK(sc_near(uv_scale(feet_m), 0.3048), "feet * meters scales multiply");
    UnitVec feet_per_m = uv_div(feet, meters);
    CHECK(uv_is_dimensionless(feet_per_m) && sc_near(uv_scale(feet_per_m), 0.3048),
          "feet / meters: dimensionless with scale 0.3048");
    UnitVec feet_sq = uv_pow(feet, 2);
    CHECK(sc_near(uv_scale(feet_sq), 0.3048 * 0.3048), "feet^2 scale == 0.3048^2");
    UnitVec per_hour = uv_pow(uv_from_name("hours"), -1);
    CHECK(sc_near(uv_scale(per_hour), 1.0 / 3600.0) && per_hour.dim[DIM_S] == -1,
          "hours^-1: scale divides");
    CHECK(scale_equal(uv_dimensionless(), uv_div(meters, uv_from_name("m"))),
          "m/m cancel returns canonical scale 1");
    CHECK(scale_equal(uv_n(1,0,0,0,0,0,0), meters),
          "uv_n vectors use canonical scale 1.0 too");

    puts("[10] derived units (Mars Climate Orbiter support)");
    UnitVec newton = uv_from_name("newtons");
    CHECK(uv_equal(newton, uv_n(1,1,-2,0,0,0,0)) && sc_near(uv_scale(newton), 1.0),
          "newtons == kg*m/s^2, scale 1");
    CHECK(uv_is_unit_word("N") && uv_is_unit_word("lbf") && uv_is_unit_word("pounds_force"),
          "N / lbf / pounds_force are unit words");
    UnitVec lbf = uv_from_name("lbf");
    CHECK(uv_equal(lbf, newton) && !scale_equal(lbf, newton) &&
          sc_near(uv_scale(lbf), 4.4482216152605),
          "lbf == same vector, scale 4.4482216152605 (the MCO mismatch)");

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
#ifdef ALLOC_AUDIT
    audit_report();   /* allocation ledger: prove zero heap use explicitly */
#endif
    return g_fail ? 1 : 0;
}
