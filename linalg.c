#include "linalg.h"

#include <math.h>


float cross_product2D(Vec2 v, Vec2 u)
{
	return v.x*u.y - v.y*u.x; 
}

Vec3 vec3(Vec4 v)
{ return (Vec3){v.x, v.y, v.z}; }

Vec4 vec4(Vec3 v)
{ return (Vec4){v.x, v.y, v.z, 1.0f}; }

float vec_length(Vec3 v)
{
	return sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

float dot(Vec3 v, Vec3 u)
{
	return v.x*u.x + v.y*u.y + v.z*u.z;
}

Vec3 cross_product(Vec3 v, Vec3 u)
{
	return (Vec3){
		v.y*u.z - v.z*u.y, 
		v.z*u.x - v.x*u.z, 
		v.x*u.y - v.y*u.x};
}

Mat3 trans_mat3(Mat3 const *mat)
{
	return (Mat3){
		mat->x1, mat->y1, mat->z1,
		mat->x2, mat->y2, mat->z2,
		mat->x3, mat->y3, mat->z3,
	};
}

Mat4 translate(float x, float y, float z)
{
	return (Mat4){
		1.0f, 0.0f, 0.0f, x,
		0.0f, 1.0f, 0.0f, y,
		0.0f, 0.0f, 1.0f, z,
		0.0f, 0.0f, 0.0f, 1.0f
	};
}

Mat4 scale(float x, float y, float z)
{
	return (Mat4){
		x, 0.0f, 0.0f, 0.0f,
		0.0f, y, 0.0f, 0.0f,
		0.0f, 0.0f, z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}

Mat4 rotate_x(float r)
{
	return (Mat4){
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, cos(r), -sin(r), 0.0f,
		0.0f, sin(r), cos(r), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}

Mat4 rotate_y(float r)
{
	return (Mat4){
		cos(r), 0.0f, sin(r), 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		-sin(r), 0.0f, cos(r), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}

Mat4 rotate_z(float r)
{
	return (Mat4){
		cos(r), -sin(r), 0.0f, 0.0f,
		sin(r), cos(r), 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
}

Mat4 trans_mat4(Mat4 const *mat)
{
	return (Mat4){
		mat->x1, mat->y1, mat->z1, mat->t1,
		mat->x2, mat->y2, mat->z2, mat->t2,
		mat->x3, mat->y3, mat->z3, mat->t3,
		mat->x4, mat->y4, mat->z4, mat->t4,
	};
}

Mat4 inverse_mat4(Mat4 const *mat)
{
	// Taken from raylibs raymath.h
	Mat4 r = {0};

	float a00 = mat->x1, a01 = mat->x2, a02 = mat->x3, a03 = mat->x4;
	float a10 = mat->y1, a11 = mat->y2, a12 = mat->y3, a13 = mat->y4;
	float a20 = mat->z1, a21 = mat->z2, a22 = mat->z3, a23 = mat->z4;
	float a30 = mat->t1, a31 = mat->t2, a32 = mat->t3, a33 = mat->t4;

	float b00 = a00*a11 - a01*a10;
	float b01 = a00*a12 - a02*a10;
	float b02 = a00*a13 - a03*a10;
	float b03 = a01*a12 - a02*a11;
	float b04 = a01*a13 - a03*a11;
	float b05 = a02*a13 - a03*a12;
	float b06 = a20*a31 - a21*a30;
	float b07 = a20*a32 - a22*a30;
	float b08 = a20*a33 - a23*a30;
	float b09 = a21*a32 - a22*a31;
	float b10 = a21*a33 - a23*a31;
	float b11 = a22*a33 - a23*a32;

	float det =
		b00*b11 - b01*b10 + b02*b09 +
		b03*b08 - b04*b07 + b05*b06;

	if (fabsf(det) < 1e-8f)
		return r;

	float inv_det = 1.0f / det;

	r.x1 = ( a11*b11 - a12*b10 + a13*b09) * inv_det;
	r.x2 = (-a01*b11 + a02*b10 - a03*b09) * inv_det;
	r.x3 = ( a31*b05 - a32*b04 + a33*b03) * inv_det;
	r.x4 = (-a21*b05 + a22*b04 - a23*b03) * inv_det;

	r.y1 = (-a10*b11 + a12*b08 - a13*b07) * inv_det;
	r.y2 = ( a00*b11 - a02*b08 + a03*b07) * inv_det;
	r.y3 = (-a30*b05 + a32*b02 - a33*b01) * inv_det;
	r.y4 = ( a20*b05 - a22*b02 + a23*b01) * inv_det;

	r.z1 = ( a10*b10 - a11*b08 + a13*b06) * inv_det;
	r.z2 = (-a00*b10 + a01*b08 - a03*b06) * inv_det;
	r.z3 = ( a30*b04 - a31*b02 + a33*b00) * inv_det;
	r.z4 = (-a20*b04 + a21*b02 - a23*b00) * inv_det;

	r.t1 = (-a10*b09 + a11*b07 - a12*b06) * inv_det;
	r.t2 = ( a00*b09 - a01*b07 + a02*b06) * inv_det;
	r.t3 = (-a30*b03 + a31*b01 - a32*b00) * inv_det;
	r.t4 = ( a20*b03 - a21*b01 + a22*b00) * inv_det;

	return r;
}

Vec4 transform(Vec4 v, Mat4 mat)
{
	return (Vec4){
		mat.x1*v.x + mat.x2*v.y + mat.x3*v.z + mat.x4*v.t,
		mat.y1*v.x + mat.y2*v.y + mat.y3*v.z + mat.y4*v.t,
		mat.z1*v.x + mat.z2*v.y + mat.z3*v.z + mat.z4*v.t,
		mat.t1*v.x + mat.t2*v.y + mat.t3*v.z + mat.t4*v.t,	
	};
}

