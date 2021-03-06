// naive program to register two color images
// method: find the translation that minimizes their L2 distance

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "iio.h"


// auxiliary function to get the value of an image at any point (i,j)
// (points outisde the original domain get the value 0)
//
// x: image data
// w: width
// h: height
// i: horizontal position
// j: vertical position
//
// return value: color of the requested sample
//
float getsample_0(float *x, int w, int h, int pd, int i, int j, int l)
{
	if (i < 0 || j < 0 || i >= w || j >= h || l < 0 || l >= pd)
		return 0;
	else
		return x[pd*(j*w+i)+l];
}


// apply a translation to the given image
//
// x: input image
// w: width
// h: height
// dx: horizontal displacement
// dy: vertical displacement
// y: output image, to be filled-in
//
void apply_translation(float *y, int dx, int dy, float *x, int w, int h, int pd)
{
	for (int j = 0; j < h; j++)
	for (int i = 0; i < w; i++)
	for (int l = 0; l < pd; l++)
	{
		int ii = i - dx;
		int jj = j - dy;
		y[(j*w+i)*pd+l] = getsample_0(x, w, h, pd, ii, jj, l);
	}
}


// zoom-out by a factor 2
//
// in: input image
// iw: input image width
// ih: input image height
// out: output image to be filled-in
// ow: output image width (supplied by the user)
// oh: output image height (supplied by the user)
//
static void zoom_out_by_factor_two(float *out, int ow, int oh,
		float *in, int iw, int ih, int pd)
{
	assert(abs(2*ow-iw) < 2);
	assert(abs(2*oh-ih) < 2);
	for (int j = 0; j < oh; j++)
	for (int i = 0; i < ow; i++)
	for (int l = 0; l < pd; l++)
	{
		float a[4];
		a[0] = getsample_0(in, iw, ih, pd, 2*i  , 2*j  , l);
		a[1] = getsample_0(in, iw, ih, pd, 2*i+1, 2*j  , l);
		a[2] = getsample_0(in, iw, ih, pd, 2*i  , 2*j+1, l);
		a[3] = getsample_0(in, iw, ih, pd, 2*i+1, 2*j+1, l);
		out[(ow*j + i)*pd+l] = (a[0] + a[1] + a[2] + a[3])/4;
	}
}

// evaluate the L2 distance between the image A and the image B translated by d
//
// A, B: input images
// w: width
// h: heigth
// d: translation vector
//
// return value: normalized L2 distance
//
float eval_displacement(float *A, float *B, int w, int h, int pd, int d[2])
{
	long double r = 0;
	int woff = w/16;
	int hoff = h/16;
	int npoints = (w-2*woff)*(h-2*hoff)*pd;
	for (int j = hoff; j < h-hoff; j++)
	for (int i = woff; i < w-woff; i++)
	for (int l = 0; l < pd; l++)
	{
		long double a = getsample_0(A, w, h, pd, i     , j     , l);
		long double b = getsample_0(B, w, h, pd, i-d[0], j-d[1], l);
		r += (a - b) * (a - b)/(1.0*npoints);
	}
	r = sqrt(r);
	return r;
}


// recursive function to find the displacement vector that minimizes error
//
// A, B: input images
// w: width
// h: heigth
// scale: number of multi-scale recursions
// d: optimal displacement (output)
//
void find_displacement(int d[2], float *A, float *B, int w, int h, int pd,
		int scale)
{
	// find an initial rhough displacement d
	if (scale > 1) // call the function recursively
	{
		int ws = ceil(w/2.0);
		int hs = ceil(h/2.0);
		float *As = malloc(ws * hs * pd * sizeof*As);
		float *Bs = malloc(ws * hs * pd * sizeof*Bs);
		zoom_out_by_factor_two(As, ws, hs, A, w, h, pd);
		zoom_out_by_factor_two(Bs, ws, hs, B, w, h, pd);
		find_displacement(d, As, Bs, ws, hs, pd, scale-1);
		free(As);
		free(Bs);
		d[0] *= 2;
		d[1] *= 2;
	} else {
		d[0] = 0;
		d[1] = 0;
	}

	// refine the rhough displacement by local optimization
	int bestn = -1, neig[9][2] = { {-1,-1},{-1,0},{-1,1},
		{0,-1},{0,0},{0,1}, {1,-1},{1,0},{1,1}, };
	float best = INFINITY;
	for (int n = 0; n < 9; n++)
	{
		int D[2] = {d[0] + neig[n][0], d[1] + neig[n][1]};
		float r = eval_displacement(A, B, w, h, pd, D);
		if (r < best) {
			best = r;
			bestn = n;
		}
	}
	d[0] += neig[bestn][0];
	d[1] += neig[bestn][1];
	fprintf(stderr, "%dx%d: %d %d\n", w, h, d[0], d[1]);
}


// register two images
//
// w: width
// h: height
// left: left image
// right: right image
// out: right image after registration
//
void cregistration(float *out, float *left, float *right, int w, int h, int pd)
{
	int d[2];
	find_displacement(d, left, right, w, h, pd, 10);
	apply_translation(out, d[0], d[1], right, w, h, pd);
}


// main function
int main(int argc, char **argv)
{
	// process input arguments
	if (argc != 4) {
		fprintf(stderr, "usage:\n\t%s left right Tright\n", *argv);
		//                          0 1    2     3
		return 1;
	}
	char *filename_left = argv[1];
	char *filename_right = argv[2];
	char *filename_Tright = argv[3];

	// read input image (assume they have the same size, do not check)
	int w, h, pd;
	float *left  = iio_read_image_float_vec(filename_left , &w, &h, &pd);
	float *right = iio_read_image_float_vec(filename_right, &w, &h, &pd);

	// allocate space for the output image
	float *out = malloc(w*h*pd*sizeof(float));

	// run the algorithm
	cregistration(out, left, right, w, h, pd);

	// save the output image
	iio_save_image_float_vec(filename_Tright, out, w, h, pd);

	// cleanup and exit
	free(left);
	free(right);
	free(out);
	return 0;
}
