#pragma once


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdbool.h>
#ifndef VEC_API
#define VEC_API static inline
#endif

#define swap_stuff(type, a, b)                                       \
    {                                                                \
        type temporary_variable = (a);                               \
        (a) = (b);                                                   \
        (b) = temporary_variable;                                    \
    }                           \



union Vec2 {
    float comps[2];
    struct {
        float x;
        float y;
    };
    struct {
        float u;
        float v;
    };
};
typedef union Vec2 Vec2;


VEC_API Vec2 vec2_sub(Vec2 a, Vec2 b) {
    return (Vec2){ a.x - b.x, a.y - b.y};
}

VEC_API Vec2 vec2_add(Vec2 a, Vec2 b) {
    return (Vec2){ a.x + b.x, a.y + b.y };
}

VEC_API Vec2 vec2_scale_fl(Vec2 v, float s) {
    return (Vec2){ v.x * s, v.y * s };
}

VEC_API Vec2 vec2_scale_vec(Vec2 v1, Vec2 v2) {
  return (Vec2){ v1.x * v2.x, v1.y * v2.y};
}

VEC_API float vec2_magnitude(Vec2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}
VEC_API Vec2 vec2_comp_inverse(Vec2 v){
  return (Vec2){.x = 1.f/v.x, .y = 1.f/v.y};
}

VEC_API Vec2 vec2_rotate_fl(Vec2 v, float angle){
  const float c = cosf(angle);
  const float s = sinf(angle);
  return (Vec2){
    .x = v.x * c - v.y * s,
    .y = v.x * s + v.y * c
  };
}

//TODO:: Make other 'normalize' fxns also return 0 on too little magnitude
VEC_API Vec2 vec2_normalize(Vec2 v){
  const float m = vec2_magnitude(v);
  if(m <= 2 * 10e-6){
    return (Vec2){0};
  }
  return (Vec2){.x = v.x/m, .y = v.y/m};
}


VEC_API float vec2_dot(Vec2 a, Vec2 b) {
  return a.x * b.x + a.y * b.y;
}
union Vec3 {
    float comps[3];
    struct {
        float x;
        float y;
        float z;
    };
    struct {
        float r;
        float g;
        float b;
    };
};
typedef union Vec3 Vec3;

static float g_vec_epsilon = 0.00001f;

VEC_API bool vec3_equality(Vec3 a, Vec3 b) {
    return (fabsf(a.x - b.x) < g_vec_epsilon) &&
      (fabsf(a.y - b.y) < g_vec_epsilon) &&
      (fabsf(a.z - b.z) < g_vec_epsilon);
}

//Vec3 to Vec2 swizzle
VEC_API Vec2 vec3_xy(Vec3 vec){
  return (Vec2){.x = vec.x, .y= vec.y};
}

VEC_API Vec3 vec3_to_radians(Vec3 degrees) {
    return (Vec3){ degrees.x * M_PI / 180.f,
                   degrees.y * M_PI / 180.f,
                   degrees.z * M_PI / 180.f };
}

VEC_API Vec3 vec3_to_degrees(Vec3 radians) {
    
    return (Vec3){ radians.x * M_1_PI * 180.f,
                   radians.y * M_1_PI * 180.f,
                   radians.z * M_1_PI * 180.f };
}

VEC_API float vec3_magnitude(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

VEC_API Vec3 vec3_normalize(Vec3 v) {
    float mag = vec3_magnitude(v);
    return (Vec3){ .x = v.x / mag, .y = v.y / mag, .z = v.z / mag };
}

VEC_API Vec3 vec3_scale_fl(Vec3 v, float s) {
    return (Vec3){ v.x * s, v.y * s, v.z * s };
}

VEC_API Vec3 vec3_neg(Vec3 v) {
    return (Vec3){
        -v.x,
        -v.y,
        -v.z,
    };
}

VEC_API Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        b.x * a.z - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

VEC_API float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

VEC_API Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){ a.x - b.x, a.y - b.y, a.z - b.z };
}

VEC_API Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){ a.x + b.x, a.y + b.y, a.z + b.z };
}

VEC_API Vec3 vec3_add_3(Vec3 a, Vec3 b, Vec3 c) {
    return (Vec3){
        a.x + b.x + c.x ,
        a.y + b.y + c.y ,
        a.z + b.z + c.z ,
    };
}
VEC_API Vec3 vec3_add_4(Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
    return (Vec3){
        a.x + b.x + c.x + d.x,
        a.y + b.y + c.y + d.y,
        a.z + b.z + c.z + d.z,
    };
}
VEC_API Vec3 vec3_orthogonal(Vec3 v) {
    float x = fabsf(v.x);
    float y = fabsf(v.y);
    float z = fabsf(v.z);

    Vec3 other = (x < y)
      ? ((x < z) ? (Vec3){ .x = 1.f } : (Vec3){ .z = 1.f })
      : ((y < z) ? (Vec3){ .y = 1.f } : (Vec3){ .z = 1.f });
    return vec3_cross(v, other);
}

//As a quaternion , w = scalar part
union Vec4 {
    float comps[4];
    struct {
        float x;
        float y;
        float z;
        float w;
    };
    struct {
        float r;
        float g;
        float b;
        float a;
    };
};
typedef union Vec4 Vec4;
typedef const Vec4 CVec4;

VEC_API Vec4 vec4_from_vec3(Vec3 v, float w) {
    return (Vec4){ .x = v.x, .y = v.y, .z = v.z, .w = w };
}

typedef float Mat4Arr[4][4];
union Mat4 {
    Mat4Arr mat;
    Vec4 cols[4];
    float cells[16];
};
typedef union Mat4 Mat4;
typedef const Mat4 CMat4;


// Creates a transpose matrix, need to cast to Mat4 if not
// initializing
#define mat4_arr_get_transpose(src)                                \
    {{                                                             \
        { src[0][0], src[1][0], src[2][0], src[3][0] },            \
        { src[0][1], src[1][1], src[2][1], src[3][1] },            \
        { src[0][2], src[1][2], src[2][2], src[3][2] },            \
        { src[0][3], src[1][3], src[2][3], src[3][3] }             \
    }}

#define MAT4_IDENTITIY                                             \
    {{                                                             \
        { 1.0f, 0.0f, 0.0f, 0.0f },                                \
        { 0.0f, 1.0f, 0.0f, 0.0f },                                \
        { 0.0f, 0.0f, 1.0f, 0.0f },                                \
        { 0.0f, 0.0f, 0.0f, 1.0f }                                 \
    }}

VEC_API float vec4_dot_vec(CVec4 a, CVec4 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

VEC_API Vec4 vec4_scale_fl(CVec4 vec, const float f) {
    return (Vec4){ vec.x * f, vec.y * f, vec.z * f, vec.w * f };
}

VEC_API Vec4 vec4_scale_vec(CVec4 v1, CVec4 v2) {
    return (Vec4){ v1.x * v2.x, v1.y * v2.y, v1.z * v2.z, v1.w * v2.w };
}

VEC_API Vec4 vec4_add_vec(CVec4 a, CVec4 b) {
    return (Vec4){ a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w };
}

VEC_API Vec4 vec4_sub_vec(CVec4 a, CVec4 b) {
    return (Vec4){ a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w };
}

VEC_API Vec4 vec4_add_vec_4(CVec4 a, CVec4 b, CVec4 c, CVec4 d) {
    return (Vec4){ a.x + b.x + c.x + d.x, a.y + b.y + c.y + d.y,
                   a.z + b.z + c.z + d.z, a.w + b.w + c.w + d.w };
}

VEC_API float vec4_mag(Vec4 v) { return sqrtf(vec4_dot_vec(v, v)); }

VEC_API Vec4 vec4_normalize(Vec4 v) {
    return vec4_scale_fl(v, 1.f / vec4_mag(v));
}

VEC_API Vec3 vec3_from_vec4(Vec4 v) {
    return (Vec3){
        v.x,
        v.y,
        v.z,
    };
}

VEC_API Vec4 vec4q_from_rot_vf(Vec3 norm_vec, float angle) {
    float c2 = cosf(angle / 2);
    float s2 = sinf(angle / 2);


    norm_vec = vec3_scale_fl(norm_vec, s2);

    return (Vec4){ norm_vec.x, norm_vec.y, norm_vec.z, c2 };
}

VEC_API Vec4 vec4q_conjugate(Vec4 q) {
    return (Vec4){ -q.x, -q.y, -q.z, q.w };
}

VEC_API Vec4 vec4q_mul_vecq(Vec4 q1, Vec4 q2) {

    Vec3 v = vec3_cross(vec3_from_vec4(q1), vec3_from_vec4((q2)));
    v.x += q1.w * q2.x + q1.x * q2.w;
    v.y += q1.w * q2.y + q1.y * q2.w;
    v.z += q1.w * q2.z + q1.z * q2.w;

    return vec4_from_vec3(
      v,
      q1.w * q2.w - vec3_dot(vec3_from_vec4(q1), vec3_from_vec4(q2)));

}

VEC_API Vec4 vec4q_inverse(Vec4 q) {
    return vec4_scale_fl(vec4q_conjugate(q), 1.f / vec4_mag(q));
}

//Not necessary to be normalized
VEC_API Vec4 vec4q_rotation_vec(Vec3 org, Vec3 dst) {
    float dot = vec3_dot(org, dst);
    float len_prod = sqrtf(vec3_dot(org, org) * vec3_dot(dst, dst));
    if (fabsf(dot/len_prod + 1.f) < g_vec_epsilon) {
        return vec4_from_vec3(vec3_normalize(vec3_orthogonal(org)),
                              0.f);
    } else {
        return vec4_normalize(
          vec4_from_vec3(vec3_cross(org, dst), dot + len_prod));
    }
}

VEC_API Vec3 vec4q_rotate_v3(Vec4 quat, Vec3 vec) {
    return vec3_from_vec4(
      vec4q_mul_vecq(quat,
                     vec4q_mul_vecq(vec4_from_vec3(vec, 0.f),
                                    vec4q_conjugate(quat))));
}

VEC_API Vec4 mat4_multiply_vec(CMat4 *mat, CVec4 v) {
    return vec4_add_vec_4(vec4_scale_fl(mat->cols[0], v.x),
                          vec4_scale_fl(mat->cols[1], v.y),
                          vec4_scale_fl(mat->cols[2], v.z),
                          vec4_scale_fl(mat->cols[3], v.w));    
}

VEC_API Mat4 mat4_multiply_mat(CMat4 *a, CMat4 *b) {

    return (Mat4){ .cols[0] = mat4_multiply_vec(a, b->cols[0]),
                   .cols[1] = mat4_multiply_vec(a, b->cols[1]),
                   .cols[2] = mat4_multiply_vec(a, b->cols[2]),
                   .cols[3] = mat4_multiply_vec(a, b->cols[3]) };

}

VEC_API Mat4 mat4_multiply_mat_3(CMat4 *a, CMat4 *b, CMat4 *c) {
    Mat4 temp = mat4_multiply_mat(b, c);
    return mat4_multiply_mat(a, &temp);
}


//TODO::complete this function implementation
//If null is given to out, only determinant is given,
//Else if matrix is non singular, out will have output matrix,
//Else it may be invalid matrix
VEC_API float mat4_det_inv(Mat4 a, Mat4 *out) {
    Mat4 b = MAT4_IDENTITIY;

    //Calcuate determinant
    float deter = 1.f;

    //Forward elimination
    for (int i = 0; i < 4; ++i) {

        int nz = i;
        for (; nz < 4; ++nz)
            if (fabsf(a.mat[i][i]) < g_vec_epsilon)
                break;
        if (nz == 4)
            return 0.f;
        if (nz != i)
            swap_stuff(Vec4, a.cols[i], a.cols[nz]);
        deter *= a.mat[i][i];

        for(int j = i+1; j < 4; ++j) {
            float fac = a.mat[j][i] / a.mat[i][i];
            for (int k = i; k < 4; ++k)
                a.mat[j][k] -= fac * a.mat[i][k];
            for (int k = 0; k < 4; ++k)
                b.mat[j][k] -= fac * b.mat[i][k];
        }
    }
    
    if (!out)
        return deter;

    //Backward elimination, with identity making
    for (int ii = 0; ii < 4; ++ii) {
        int i = 4 - ii - 1;
        for (int j = 0; j < i; ++j) {
            float fac = a.mat[j][i] / a.mat[i][i];
            for (int k = j+1; k < 4; ++k)
                a.mat[j][k] -= fac * a.mat[i][k];
            for (int k = 0; k < 4; ++k)
                b.mat[j][k] -= fac * b.mat[i][k];
        }
        for (int j = 0; j < 4; ++j)
            b.mat[i][j] /= a.mat[i][i];
        a.mat[i][i] = 1.f;
    }
    *out = b;
    return deter;
}

//Form transformation matrices, radians expected
VEC_API Mat4 mat4_rotation_Z(float angle) {

    float c = cosf(angle);
    float s = sinf(angle);

    return (Mat4){ { { c, s, 0.f, 0.f },
                     { -s, c, 0.f, 0.f },
                     { 0.f, 0.f, 1.f, 0.f },
                     { 0.f, 0.f, 0.f, 1.f } } };

}

VEC_API Mat4 mat4_rotation_X(float angle) {

    float c = cosf(angle);
    float s = sinf(angle);
    return (Mat4){ { { 1.f, 0.f, 0.f, 0.f },
                     { 0.f, c, s, 0.f },
                     { 0.f, -s, c, 0.f },
                     { 0.f, 0.f, 0.f, 1.f } } };

}

VEC_API Mat4 mat4_rotation_Y(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    return (Mat4){ { { c, 0.f, -s, 0.f },
                     { 0.f, 1.f, 0.f, 0.f },
                     { s, 0.f, c, 0.f },
                     { 0.f, 0.f, 0.f, 1.f } } };

}

VEC_API Mat4 mat4_rotation_XYZ(Vec3 angles) {
    Mat4 rx = mat4_rotation_X(angles.x);
    Mat4 ry = mat4_rotation_Y(angles.y);
    Mat4 rz = mat4_rotation_Z(angles.z);

    return mat4_multiply_mat_3(&rz, &ry, &rx);
}

VEC_API Mat4 mat4_rotation_ZYX(Vec3 angles) {
    Mat4 rx = mat4_rotation_X(angles.x);
    Mat4 ry = mat4_rotation_Y(angles.y);
    Mat4 rz = mat4_rotation_Z(angles.z);

    return mat4_multiply_mat_3(&rx, &ry, &rz);
}

VEC_API Mat4 mat4_scale_3(float sx, float sy, float sz) {
    return (Mat4){ { { sx, 0.f, 0.f, 0.f },
                     { 0.f, sy, 0.f, 0.f },
                     { 0.f, 0.f, sz, 0.f },
                     { 0.f, 0.f, 0.f, 1.f } } };
}

VEC_API Mat4 mat4_translate_3(float tx, float ty, float tz) {
    return (Mat4){ { { 1.f, 0.f, 0.f, 0.f },
                     { 0.f, 1.f, 0.f, 0.f },
                     { 0.f, 0.f, 1.f, 0.f },
                     { tx, ty, tz, 1.f } } };
}

//Linearly Maps world _min, world _max to -1,-1,0 to 1,1,1
VEC_API Mat4 mat4_orthographic(Vec3 world_min, Vec3 world_max) {

    Vec4 min = { world_min.x, world_min.y, world_min.z };
    Vec4 max = { world_max.x, world_max.y, world_max.z };

    Vec4 sum = vec4_add_vec(min, max);
    Vec4 diff = vec4_sub_vec(max, min);

    sum = vec4_scale_fl(sum, 0.5f);
    diff = vec4_scale_vec(diff, (Vec4){ 0.5f, 0.5f, 1.f, 1.f });

    Mat4 mat1 = mat4_scale_3(1.f/diff.x, 1.f/diff.y, 1/diff.z);
    Mat4 mat2 = mat4_translate_3(-sum.x, -sum.y, -world_min.z);

    return mat4_multiply_mat(&mat1, &mat2);
}

//fov in radians
//Gives a transformation matrix to apply, uses orthographic too in itself
VEC_API Mat4 mat4_perspective(Vec3 world_min, Vec3 world_max,
                             float fovx) {
    Mat4 ortho = mat4_orthographic(world_min, world_max);
    float D = 1.f / tanf(fovx/2.f);
    Mat4 temp = { {
      { D, 0.f, 0.f, 0.f },
      { 0.f, D, 0.f, 0.f },
      { 0.f, 0.f, 1 + D, 1.f },
      { 0.f, 0.f, 0.f, D },
    } };
    return mat4_multiply_mat(&temp, &ortho);
}


#pragma clang diagnostic pop
