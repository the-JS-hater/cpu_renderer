#pragma once

typedef struct
{ float x, y; } Vec2;

typedef struct
{ float x, y, z; } Vec3;

typedef struct
{ float x, y, z, t; } Vec4;

typedef struct
{
	float x1, x2, x3;
	float y1, y2, y3;
	float z1, z2, z3;
} Mat3;

typedef struct
{
	float x1, x2, x3, x4;
	float y1, y2, y3, y4;
	float z1, z2, z3, z4;
	float t1, t2, t3, t4;
} Mat4;

float cross_product2D(Vec2 v, Vec2 u);
Vec3 vec3(Vec4 v);
Vec4 vec4(Vec3 v);
float vec_length(Vec3 v);
float dot(Vec3 v, Vec3 u);
Vec3 cross_product(Vec3 v, Vec3 u);
Mat3 trans_mat3(Mat3 const *mat);
Mat4 translate(float x, float y, float z);
Mat4 scale(float x, float y, float z);
Mat4 rotate_x(float r);
Mat4 rotate_y(float r);
Mat4 rotate_z(float r);
Mat4 trans_mat4(Mat4 const *mat);
Mat4 inverse_mat4(Mat4 const *mat);
Vec4 transform(Vec4 v, Mat4 mat);
