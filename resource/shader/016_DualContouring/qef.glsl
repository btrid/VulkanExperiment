// minimal SVD implementation for calculating feature points from hermite data
// public domain

//#define mat3x3_tri float
#define SVD_NUM_SWEEPS 5

// SVD
////////////////////////////////////////////////////////////////////////////////

#define PSUEDO_INVERSE_THRESHOLD (0.1hf)
#define vec4 f16vec4
#define mat3x3 f16mat3
#define float float16_t

void svd_mul_matrix_vec(out vec4 result, mat3x3 a, vec4 b)
{
	result.x = dot(vec4(a[0][0], a[0][1], a[0][2], 0.hf), b);
	result.y = dot(vec4(a[1][0], a[1][1], a[1][2], 0.hf), b);
	result.z = dot(vec4(a[2][0], a[2][1], a[2][2], 0.hf), b);
	result.w = 0.hf;
}

void givens_coeffs_sym(float a_pp, float a_pq, float a_qq, out float c, out float s) 
{
	if (a_pq == 0.hf)
    {
		c = 1.hf;
		s = 0.hf;
		return;
	}
	float tau = (a_qq - a_pp) / (2.hf * a_pq);
	float stt = sqrt(1.hf + tau * tau);
	float tan = 1.hf / ((tau >= 0.hf) ? (tau + stt) : (tau - stt));
	c = 1.hf/sqrt(1.hf + tan * tan);
	s = tan * c;
}

void svd_rotate_xy(inout float x, inout float y, in float c, in float s) 
{
	float u = x; float v = y;
	x = c * u - s * v;
	y = s * u + c * v;
}

void svd_rotateq_xy(inout float x, inout float y, in float a, float c, float s) 
{
	float cc = c * c; float ss = s * s;
	float mx = 2.hf * c * s * a;
	float u = x; float v = y;
	x = cc * u - mx + ss * v;
	y = ss * u + mx + cc * v;
}

void svd_rotate(inout mat3x3 vtav, out mat3x3 v, int a, int b) 
{
	if (vtav[a][b] == 0.hf) return;
	
	float c, s;
	givens_coeffs_sym(vtav[a][a], vtav[a][b], vtav[b][b], c, s);

	float x, y, z;
	x = vtav[a][a]; y = vtav[b][b]; z = vtav[a][b];
	svd_rotateq_xy(x,y,z,c,s);
	vtav[a][a] = x; vtav[b][b] = y; vtav[a][b] = z;

	x = vtav[0][3-b]; y = vtav[1-a][2];
	svd_rotate_xy(x, y, c, s);
	vtav[0][3-b] = x; vtav[1-a][2] = y;

	vtav[a][b] = 0.hf;
	
	x = v[0][a]; y = v[0][b];
	svd_rotate_xy(x, y, c, s);
	v[0][a] = x; v[0][b] = y;

	x = v[1][a]; y = v[1][b];
	svd_rotate_xy(x, y, c, s);
	v[1][a] = x; v[1][b] = y;

	x = v[2][a]; y = v[2][b];
	svd_rotate_xy(x, y, c, s);
	v[2][a] = x; v[2][b] = y;
}

void svd_solve_sym(float a[6], out vec4 sigma, in mat3x3 v) 
{
	// assuming that A is symmetric: can optimize all operations for 
	// the upper right triagonal
	mat3x3 vtav;
	vtav[0][0] = a[0]; vtav[0][1] = a[1]; vtav[0][2] = a[2]; 
	vtav[1][0] = 0.hf; vtav[1][1] = a[3]; vtav[1][2] = a[4]; 
	vtav[2][0] = 0.hf; vtav[2][1] = 0.hf; vtav[2][2] = a[5];

	// assuming V is identity: you can also pass a matrix the rotations
	// should be applied to. (U is not computed)
	for (int i = 0; i < SVD_NUM_SWEEPS; ++i) 
    {
		svd_rotate(vtav, v, 0, 1);
		svd_rotate(vtav, v, 0, 2);
		svd_rotate(vtav, v, 1, 2);
	}

	sigma = vec4(vtav[0][0], vtav[1][1], vtav[2][2], 0.hf);
}

float svd_invdet(float x, float tol) 
{
	return (abs(x) < tol || abs(1.hf / x) < tol) ? 0.hf : (1.hf / x);
}

void svd_pseudoinverse(out mat3x3 o, in vec4 sigma, in mat3x3 v) 
{
	float d0 = svd_invdet(sigma.x, PSUEDO_INVERSE_THRESHOLD);
	float d1 = svd_invdet(sigma.y, PSUEDO_INVERSE_THRESHOLD);
	float d2 = svd_invdet(sigma.z, PSUEDO_INVERSE_THRESHOLD);

	o[0][0] = v[0][0] * d0 * v[0][0] + v[0][1] * d1 * v[0][1] + v[0][2] * d2 * v[0][2];
	o[0][1] = v[0][0] * d0 * v[1][0] + v[0][1] * d1 * v[1][1] + v[0][2] * d2 * v[1][2];
	o[0][2] = v[0][0] * d0 * v[2][0] + v[0][1] * d1 * v[2][1] + v[0][2] * d2 * v[2][2];
	o[1][0] = v[1][0] * d0 * v[0][0] + v[1][1] * d1 * v[0][1] + v[1][2] * d2 * v[0][2];
	o[1][1] = v[1][0] * d0 * v[1][0] + v[1][1] * d1 * v[1][1] + v[1][2] * d2 * v[1][2];
	o[1][2] = v[1][0] * d0 * v[2][0] + v[1][1] * d1 * v[2][1] + v[1][2] * d2 * v[2][2];
	o[2][0] = v[2][0] * d0 * v[0][0] + v[2][1] * d1 * v[0][1] + v[2][2] * d2 * v[0][2];
	o[2][1] = v[2][0] * d0 * v[1][0] + v[2][1] * d1 * v[1][1] + v[2][2] * d2 * v[1][2];
	o[2][2] = v[2][0] * d0 * v[2][0] + v[2][1] * d1 * v[2][1] + v[2][2] * d2 * v[2][2];
}

void svd_solve_ATA_ATb(float ATA[6], vec4 ATb, out vec4 x) 
{
	mat3x3 V = mat3x3(1.);
//	V[0][0] = 1.hf; V[0][1] = 0.hf; V[0][2] = 0.hf;
//	V[1][0] = 0.hf; V[1][1] = 1.hf; V[1][2] = 0.hf;
//	V[2][0] = 0.hf; V[2][1] = 0.hf; V[2][2] = 1.hf;

	vec4 sigma;// = { 0.hf, 0.hf, 0.hf, 0.hf };
	svd_solve_sym(ATA, sigma, V);
	
	// A = UEV^T; U = A / (E*V^T)
	mat3x3 Vinv;
	svd_pseudoinverse(Vinv, sigma, V);
	svd_mul_matrix_vec(x, Vinv, ATb);
}

void svd_vmul_sym(out vec4 result, in float A[6], in vec4 v) 
{
	vec4 A_row_x = { A[0], A[1], A[2], 0.hf };

	result.x = dot(A_row_x, v);
	result.y = A[1] * v.x + A[3] * v.y + A[4] * v.z;
	result.z = A[2] * v.x + A[4] * v.y + A[5] * v.z;
}

// QEF
////////////////////////////////////////////////////////////////////////////////

void qef_add(in vec4 n, in vec4 p, inout float ATA[6], inout vec4 ATb, inout vec4 pointaccum)
{
	ATA[0] += n.x * n.x;
	ATA[1] += n.x * n.y;
	ATA[2] += n.x * n.z;
	ATA[3] += n.y * n.y;
	ATA[4] += n.y * n.z;
	ATA[5] += n.z * n.z;

	float b = dot(p, n);
	ATb.x += n.x * b;
	ATb.y += n.y * b;
	ATb.z += n.z * b;

	pointaccum.x += p.x;
	pointaccum.y += p.y;
	pointaccum.z += p.z;
	pointaccum.w += 1.hf;
}

float qef_calc_error(in float A[6], vec4 x, vec4 b) 
{
	vec4 tmp;
	svd_vmul_sym(tmp, A, x);
	tmp = b - tmp;

	return dot(tmp, tmp);
}

float qef_solve(float ATA[6], in vec4 ATb, in vec4 pointaccum, inout vec4 x) 
{
	vec4 masspoint = pointaccum / pointaccum.w;

	vec4 A_mp = { 0.hf, 0.hf, 0.hf, 0.hf };
	svd_vmul_sym(A_mp, ATA, masspoint);
	A_mp = ATb - A_mp;

	svd_solve_ATA_ATb(ATA, A_mp, x);

	float error = qef_calc_error(ATA, x, ATb);
	x += masspoint;
		
	return error;
}

vec4 qef_solve_from_points(in vec4 positions[12], in vec4 normals[12], int count/*,float* error*/)
{
	vec4 pointaccum = {0.hf, 0.hf, 0.hf, 0.hf};
	vec4 ATb = {0.hf, 0.hf, 0.hf, 0.hf};
	float ATA[6] = {0.hf, 0.hf, 0.hf, 0.hf, 0.hf, 0.hf};
	
	for (int i= 0; i < count; ++i)
    {
		qef_add(normals[i],positions[i], ATA, ATb, pointaccum);
	}
	
	vec4 solved_position = { 0.hf, 0.hf, 0.hf, 0.hf };
	/**error =*/ qef_solve( ATA, ATb, pointaccum, solved_position);
	return solved_position;
}
#undef vec4
#undef mat3x3
#undef float
