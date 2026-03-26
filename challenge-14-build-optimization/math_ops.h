#pragma once

double compute_vwap(double price, double qty, double prev_vwap);
double compute_spread(double price, double prev_spread);
double transform_value(double price, double qty, int index);
double compute_score(const double* values, int count);
double normalize(double x);
