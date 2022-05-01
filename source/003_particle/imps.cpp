/*=====================================================================
  mps.c
  (c) Kazuya SHIBATA, Kohei MUROTANI and Seiichi KOSHIZUKA (2014)

   Fluid Simulation Program Based on a Particle Method (the MPS method)
   Last update: May 21, 2014
=======================================================================*/
#include <003_particle/imps.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <btrlib/DefineMath.h>

namespace imps
{

#define DIM                  3
#define PARTICLE_DISTANCE    0.075
#define DT                   0.003
#define OUTPUT_INTERVAL      2 

#define ARRAY_SIZE           5000
#define KINEMATIC_VISCOSITY  (1.0E-6)
#define FLUID_DENSITY        1000.0 
#define Gravity vec3(0.0, -9.8, 0.0);
#define RADIUS_FOR_NUMBER_DENSITY  (2.1*PARTICLE_DISTANCE) 
#define RADIUS_FOR_GRADIENT        (2.1*PARTICLE_DISTANCE) 
#define RADIUS_FOR_LAPLACIAN       (3.1*PARTICLE_DISTANCE) 
#define COLLISION_DISTANCE         (0.5*PARTICLE_DISTANCE)
#define THRESHOLD_RATIO_OF_NUMBER_DENSITY  0.97   
#define COEFFICIENT_OF_RESTITUTION 0.2
#define COMPRESSIBILITY (0.45E-9)
#define EPS             (0.01 * PARTICLE_DISTANCE)     
#define ON              1
#define OFF             0
#define RELAXATION_COEFFICIENT_FOR_PRESSURE 0.2
enum ParticleType
{
    PT_Ghost,
    PT_Fluid,
    PT_Wall,
    PT_DummyWall,
};
enum BoundaryCondi
{
    BC_GhostOrDummy,
    BC_InnerParticle,
    BC_SurfaceParticle,
};
#define DIRICHLET_BOUNDARY_IS_NOT_CONNECTED 0 
#define DIRICHLET_BOUNDARY_IS_CONNECTED     1 
#define DIRICHLET_BOUNDARY_IS_CHECKED       2 

float weight(float distance, float re) {
	float weightIJ;

	if (distance >= re) {
		weightIJ = 0.0;
	}
	else {
		weightIJ = (re / distance) - 1.0;
	}
	return weightIJ;
}

void initializeParticle_for3dim(dFluid& dfluid);
void calConstantParameter(dFluid& dfluid);
void calNZeroAndLambda(dFluid& dfluid);
void calGravity(dFluid& dfluid);
void calViscosity(dFluid& dfluid);
void moveParticle(dFluid& dfluid);
void collision(dFluid& dfluid);
void calPressure(dFluid& dfluid);
void calNumberDensity(dFluid& dfluid);
void setBoundaryCondition(dFluid& dfluid);
void setSourceTerm(dFluid& dfluid);
void setMatrix(dFluid& dfluid);
void exceptionalProcessingForBoundaryCondition(dFluid& dfluid);
void checkBoundaryCondition(dFluid& dfluid);
void increaseDiagonalTerm(dFluid& dfluid);
void solveSimultaniousEquationsByGaussEliminationMethod(dFluid& dfluid);
void removeNegativePressure(dFluid& dfluid);
void setMinimumPressure(dFluid& dfluid);
void calPressureGradient(dFluid& dfluid);
void moveParticleUsingPressureGradient(dFluid& dfluid);

float Re_forNumberDensity, Re2_forNumberDensity;
float Re_forGradient, Re2_forGradient;
float Re_forLaplacian, Re2_forLaplacian;
float N0_forNumberDensity;
float N0_forGradient;
float N0_forLaplacian;
float Lambda;
float collisionDistance, collisionDistance2;
float FluidDensity;

void initializeParticle_for3dim(dFluid& dfluid) {
    int i = 0;
    int flagOfParticleGeneration;

    int nX = (int)(1.0 / PARTICLE_DISTANCE) + 5;
    int nY = (int)(0.6 / PARTICLE_DISTANCE) + 5;
    int nZ = (int)(0.3 / PARTICLE_DISTANCE) + 5;
    for (int iX = -4; iX < nX; iX++) {
        for (int iY = -4; iY < nY; iY++) {
            for (int iZ = -4; iZ < nZ; iZ++) {
                float x = PARTICLE_DISTANCE * iX;
                float y = PARTICLE_DISTANCE * iY;
                float z = PARTICLE_DISTANCE * iZ;
                int flagOfParticleGeneration = OFF;

                /* dummy wall region */
                if ((((x > -4.0 * PARTICLE_DISTANCE + EPS) && (x <= 1.00 + 4.0 * PARTICLE_DISTANCE + EPS)) && ((y > 0.0 - 4.0 * PARTICLE_DISTANCE + EPS) && (y <= 0.6 + EPS))) && ((z > 0.0 - 4.0 * PARTICLE_DISTANCE + EPS) && (z <= 0.3 + 4.0 * PARTICLE_DISTANCE + EPS))) {
                    dfluid.ParticleType[i] = PT_DummyWall;
                    flagOfParticleGeneration = ON;
                }

                /* wall region */
                if ((((x > -2.0 * PARTICLE_DISTANCE + EPS) && (x <= 1.00 + 2.0 * PARTICLE_DISTANCE + EPS)) && ((y > 0.0 - 2.0 * PARTICLE_DISTANCE + EPS) && (y <= 0.6 + EPS))) && ((z > 0.0 - 2.0 * PARTICLE_DISTANCE + EPS) && (z <= 0.3 + 2.0 * PARTICLE_DISTANCE + EPS))) {
                    dfluid.ParticleType[i] = PT_Wall;
                    flagOfParticleGeneration = ON;
                }

                /* wall region */
                if ((((x > -4.0 * PARTICLE_DISTANCE + EPS) && (x <= 1.00 + 4.0 * PARTICLE_DISTANCE + EPS)) && ((y > 0.6 - 2.0 * PARTICLE_DISTANCE + EPS) && (y <= 0.6 + EPS))) && ((z > 0.0 - 4.0 * PARTICLE_DISTANCE + EPS) && (z <= 0.3 + 4.0 * PARTICLE_DISTANCE + EPS))) {
                    dfluid.ParticleType[i] = PT_Wall;
                    flagOfParticleGeneration = ON;
                }

                /* empty region */
                if ((((x > 0.0 + EPS) && (x <= 1.00 + EPS)) && (y > 0.0 + EPS)) && ((z > 0.0 + EPS) && (z <= 0.3 + EPS))) {
                    flagOfParticleGeneration = OFF;
                }

                /* fluid region */
                if ((((x > 0.0 + EPS) && (x <= 0.25 + EPS)) && ((y > 0.0 + EPS) && (y < 0.5 + EPS))) && ((z > 0.0 + EPS) && (z <= 0.3 + EPS))) {
                    dfluid.ParticleType[i] = PT_Fluid;
                    flagOfParticleGeneration = ON;
                }

                if (flagOfParticleGeneration == ON) {
                    dfluid.Position[i] = vec3(x, y, z);
                    i++;
                }
            }
        }
    }
    dfluid.PNum = i;
    for (i = 0; i < dfluid.PNum; i++) { dfluid.Velocity[i] = vec3(0.0); }
}


void calConstantParameter(dFluid& dfluid) {

    Re_forNumberDensity = RADIUS_FOR_NUMBER_DENSITY;
    Re_forGradient = RADIUS_FOR_GRADIENT;
    Re_forLaplacian = RADIUS_FOR_LAPLACIAN;
    Re2_forNumberDensity = Re_forNumberDensity * Re_forNumberDensity;
    Re2_forGradient = Re_forGradient * Re_forGradient;
    Re2_forLaplacian = Re_forLaplacian * Re_forLaplacian;
    calNZeroAndLambda(dfluid);
    FluidDensity = FLUID_DENSITY;
    collisionDistance = COLLISION_DISTANCE;
    collisionDistance2 = collisionDistance * collisionDistance;
}

void calNZeroAndLambda(dFluid& dfluid) {
    int iX, iY, iZ;
    int iZ_start, iZ_end;
    float xj, yj, zj, distance, distance2;
    float xi, yi, zi;

    if (DIM == 2) {
        iZ_start = 0; iZ_end = 1;
    }
    else {
        iZ_start = -4; iZ_end = 5;
    }

    N0_forNumberDensity = 0.0;
    N0_forGradient = 0.0;
    N0_forLaplacian = 0.0;
    Lambda = 0.0;
    xi = 0.0;  yi = 0.0;  zi = 0.0;

    for (iX = -4; iX < 5; iX++) {
        for (iY = -4; iY < 5; iY++) {
            for (iZ = iZ_start; iZ < iZ_end; iZ++) {
                if (((iX == 0) && (iY == 0)) && (iZ == 0))continue;
                xj = PARTICLE_DISTANCE * (float)(iX);
                yj = PARTICLE_DISTANCE * (float)(iY);
                zj = PARTICLE_DISTANCE * (float)(iZ);
                distance2 = (xj - xi) * (xj - xi) + (yj - yi) * (yj - yi) + (zj - zi) * (zj - zi);
                distance = sqrt(distance2);
                N0_forNumberDensity += weight(distance, Re_forNumberDensity);
                N0_forGradient += weight(distance, Re_forGradient);
                N0_forLaplacian += weight(distance, Re_forLaplacian);
                Lambda += distance2 * weight(distance, Re_forLaplacian);
            }
        }
    }
    Lambda = Lambda / N0_forLaplacian;
}

void init(dFluid& dfluid)
{
	dfluid.Acceleration.resize(ARRAY_SIZE);
	dfluid.ParticleType.resize(ARRAY_SIZE);
	dfluid.Position.resize(ARRAY_SIZE);
	dfluid.Velocity.resize(ARRAY_SIZE);
	dfluid.Pressure.resize(ARRAY_SIZE);
	dfluid.NumberDensity.resize(ARRAY_SIZE);
	dfluid.BoundaryCondition.resize(ARRAY_SIZE);
	dfluid.SourceTerm.resize(ARRAY_SIZE);
	dfluid.FlagForCheckingBoundaryCondition.resize(ARRAY_SIZE);
	dfluid.CoefficientMatrix.resize(ARRAY_SIZE* ARRAY_SIZE);
	dfluid.MinimumPressure.resize(ARRAY_SIZE);

    initializeParticle_for3dim(dfluid);
	calConstantParameter(dfluid);
}



void run(dFluid& dfluid) {
    calGravity(dfluid);
    calViscosity(dfluid);
    moveParticle(dfluid);
    collision(dfluid);
    calPressure(dfluid);
    calPressureGradient(dfluid);
    moveParticleUsingPressureGradient(dfluid);
}


void calGravity(dFluid& dfluid) {
    int i;

    for (i = 0; i < dfluid.PNum; i++) {
        if (dfluid.ParticleType[i] == PT_Fluid) {
            dfluid.Acceleration[i] = Gravity;
        }
        else {
            dfluid.Acceleration[i] = vec3(0.);
        }
    }
}


void calViscosity(dFluid& dfluid)
{

    float a = (KINEMATIC_VISCOSITY) * (2.0 * DIM) / (N0_forLaplacian * Lambda);
    for (int i = 0; i < dfluid.PNum; i++) {
        if (dfluid.ParticleType[i] != PT_Fluid) continue;
        auto viscosityTerm = vec3(0.0);

        for (int j = 0; j < dfluid.PNum; j++) {
            if ((j == i) || (dfluid.ParticleType[j] == PT_Ghost)) continue;
            auto xij = dfluid.Position[j] - dfluid.Position[i];
            auto distance2 = dot(xij, xij);
            auto distance = sqrt(distance2);
            if (distance < Re_forLaplacian) {
                float w = weight(distance, Re_forLaplacian);
                viscosityTerm += (dfluid.Velocity[j] - dfluid.Velocity[i]) * w;
            }
        }
        viscosityTerm = viscosityTerm * a;
        dfluid.Acceleration[i] += viscosityTerm;
    }
}


void moveParticle(dFluid& dfluid)
{
    for (int i = 0; i < dfluid.PNum; i++) {
        if (dfluid.ParticleType[i] == PT_Fluid) {
            dfluid.Velocity[i] += dfluid.Acceleration[i] * DT;
            dfluid.Position[i] += dfluid.Velocity[i] * DT;
        }
        dfluid.Acceleration[i] = vec3(0.0);
    }
}


void collision(dFluid& dfluid) 
{
    float e = COEFFICIENT_OF_RESTITUTION;
    static vec3 VelocityAfterCollision[ARRAY_SIZE];

    for (int i = 0; i < dfluid.PNum; i++) {
        VelocityAfterCollision[i] = dfluid.Velocity[i];
    }
    for (int i = 0; i < dfluid.PNum; i++) {
        if (dfluid.ParticleType[i] == PT_Fluid) {
            auto mi = FluidDensity;
            vec3 velocity = dfluid.Velocity[i];
            for (int j = 0; j < dfluid.PNum; j++) {
                if ((j == i) || (dfluid.ParticleType[j] == PT_Ghost)) continue;
                auto xij = dfluid.Position[j] - dfluid.Position[i];
                auto distance2 = dot(xij,xij);
                if (distance2 < collisionDistance2) {
                    auto distance = sqrt(distance2);
                    auto forceDT = dot((velocity - dfluid.Velocity[j]) * (xij / distance), vec3(1.f));
                    if (forceDT > 0.0) {
                        auto mj = FluidDensity;
                        forceDT *= (1.0 + e) * mi * mj / (mi + mj);
                        velocity -= (forceDT / mi) * (xij / distance);
                        /*
                        if(j>i){ fprintf(stderr,"WARNING: Collision occured between %d and %d particles.\n",i,j); }
                        */
                    }
                }
            }
            VelocityAfterCollision[i] = velocity;
        }
    }
    for (int i = 0; i < dfluid.PNum; i++) {
        if (dfluid.ParticleType[i] == PT_Fluid) {
            dfluid.Position[i] += (VelocityAfterCollision[i] - dfluid.Velocity[i]) * DT;
            dfluid.Velocity[i] = VelocityAfterCollision[i];
        }
    }
}


void calPressure(dFluid& dfluid) {
    calNumberDensity(dfluid);
    setBoundaryCondition(dfluid);
    setSourceTerm(dfluid);
    setMatrix(dfluid);
    solveSimultaniousEquationsByGaussEliminationMethod(dfluid);
    removeNegativePressure(dfluid);
    setMinimumPressure(dfluid);
}


void calNumberDensity(dFluid& dfluid) {
    int    i, j;
    vec3 xij;
    float distance, distance2;
    float w;

    for (int i = 0; i < dfluid.PNum; i++) {
        dfluid.NumberDensity[i] = 0.0;
        if (dfluid.ParticleType[i] == PT_Ghost) continue;
        for (j = 0; j < dfluid.PNum; j++) {
            if ((j == i) || (dfluid.ParticleType[j] == PT_Ghost)) continue;
            xij = dfluid.Position[j] - dfluid.Position[i];
            distance2 = dot(xij, xij);
            distance = sqrt(distance2);
            w = weight(distance, Re_forNumberDensity);
            dfluid.NumberDensity[i] += w;
        }
    }
}


void setBoundaryCondition(dFluid& dfluid)
{
    float n0 = N0_forNumberDensity;
    float beta = THRESHOLD_RATIO_OF_NUMBER_DENSITY;

    for (int i = 0; i < dfluid.PNum; i++) {
        if (dfluid.ParticleType[i] == PT_Ghost || dfluid.ParticleType[i] == PT_DummyWall) {
            dfluid.BoundaryCondition[i] = BC_GhostOrDummy;
        }
        else if (dfluid.NumberDensity[i] < beta * n0) {
            dfluid.BoundaryCondition[i] = BC_SurfaceParticle;
        }
        else {
            dfluid.BoundaryCondition[i] = BC_InnerParticle;
        }
    }
}


void setSourceTerm(dFluid& dfluid) 
{
    int i;
    float n0 = N0_forNumberDensity;
    float gamma = RELAXATION_COEFFICIENT_FOR_PRESSURE;

    for (int i = 0; i < dfluid.PNum; i++) {
        dfluid.SourceTerm[i] = 0.0;
        if (dfluid.ParticleType[i] == PT_Ghost || dfluid.ParticleType[i] == PT_DummyWall) continue;
        if (dfluid.BoundaryCondition[i] == BC_InnerParticle) {
            dfluid.SourceTerm[i] = gamma * (1.0 / (DT * DT)) * ((dfluid.NumberDensity[i] - n0) / n0);
        }
        else if (dfluid.BoundaryCondition[i] == BC_SurfaceParticle) {
            dfluid.SourceTerm[i] = 0.0;
        }
    }
}


void setMatrix(dFluid& dfluid) {
    float n0 = N0_forLaplacian;
    int n = dfluid.PNum;

    for (int i = 0; i < dfluid.PNum; i++) {
        for (int j = 0; j < dfluid.PNum; j++) {
            dfluid.CoefficientMatrix[i * n + j] = 0.0;
        }
    }

    auto a = 2.0 * DIM / (n0 * Lambda);
    for (int i = 0; i < dfluid.PNum; i++) {
        if (dfluid.BoundaryCondition[i] != BC_InnerParticle) continue;
        for (int j = 0; j < dfluid.PNum; j++) {
            if ((j == i) || (dfluid.BoundaryCondition[j] == BC_GhostOrDummy)) continue;
            auto xij = dfluid.Position[j] - dfluid.Position[i];
            auto distance2 = dot(xij, xij);
            auto distance = sqrt(distance2);
            if (distance >= Re_forLaplacian)continue;
            auto coefficientIJ = a * weight(distance, Re_forLaplacian) / FluidDensity;
            dfluid.CoefficientMatrix[i * n + j] = (-1.0) * coefficientIJ;
            dfluid.CoefficientMatrix[i * n + i] += coefficientIJ;
        }
        dfluid.CoefficientMatrix[i * n + i] += (COMPRESSIBILITY) / (DT * DT);
    }
    exceptionalProcessingForBoundaryCondition(dfluid);
}


void exceptionalProcessingForBoundaryCondition(dFluid& dfluid) {
    /* If there is no Dirichlet boundary condition on the fluid,
       increase the diagonal terms of the matrix for an exception. This allows us to solve the matrix without Dirichlet boundary conditions. */
    setBoundaryCondition(dfluid);
    increaseDiagonalTerm(dfluid);
}


void checkBoundaryCondition(dFluid& dfluid) {

    for (int i = 0; i < dfluid.PNum; i++) {
        if (dfluid.BoundaryCondition[i] == BC_GhostOrDummy) {
            dfluid.FlagForCheckingBoundaryCondition[i] = BC_GhostOrDummy;
        }
        else if (dfluid.BoundaryCondition[i] == BC_SurfaceParticle) {
            dfluid.FlagForCheckingBoundaryCondition[i] = DIRICHLET_BOUNDARY_IS_CONNECTED;
        }
        else {
            dfluid.FlagForCheckingBoundaryCondition[i] = DIRICHLET_BOUNDARY_IS_NOT_CONNECTED;
        }
    }

    int count = 0;
    do {
        for (int i = 0; i < dfluid.PNum; i++) {
            if (dfluid.FlagForCheckingBoundaryCondition[i] == DIRICHLET_BOUNDARY_IS_CONNECTED) {
                for (int j = 0; j < dfluid.PNum; j++) {
                    if (j == i) continue;
                    if ((dfluid.ParticleType[j] == PT_Ghost) || (dfluid.ParticleType[j] == PT_DummyWall)) continue;
                    if (dfluid.FlagForCheckingBoundaryCondition[j] == DIRICHLET_BOUNDARY_IS_NOT_CONNECTED) {
                        auto pij = dfluid.Position[j] - dfluid.Position[i];
                        auto distance2 = dot(pij, pij);
                        if (distance2 >= Re2_forLaplacian)continue;
                        dfluid.FlagForCheckingBoundaryCondition[j] = DIRICHLET_BOUNDARY_IS_CONNECTED;
                    }
                }
                dfluid.FlagForCheckingBoundaryCondition[i] = DIRICHLET_BOUNDARY_IS_CHECKED;
                count++;
            }
        }
    } while (count != 0); /* This procedure is repeated until the all fluid or wall particles (which have Dirhchlet boundary condition in the particle group) are in the state of "DIRICHLET_BOUNDARY_IS_CHECKED".*/

    for (int i = 0; i < dfluid.PNum; i++) {
        if (dfluid.FlagForCheckingBoundaryCondition[i] == DIRICHLET_BOUNDARY_IS_NOT_CONNECTED) {
            fprintf(stderr, "WARNING: There is no dirichlet boundary condition for %d-th particle.\n", i);
        }
    }
}


void increaseDiagonalTerm(dFluid& dfluid) {
    int n = dfluid.PNum;

    for (int i = 0; i < n; i++) {
        if (dfluid.FlagForCheckingBoundaryCondition[i] == DIRICHLET_BOUNDARY_IS_NOT_CONNECTED) {
            dfluid.CoefficientMatrix[i * n + i] = 2.0 * dfluid.CoefficientMatrix[i * n + i];
        }
    }
}


void solveSimultaniousEquationsByGaussEliminationMethod(dFluid& dfluid)
{
    int n = dfluid.PNum;

    for (int i = 0; i < n; i++) {
        dfluid.Pressure[i] = 0.0;
    }
    for (int i = 0; i < n - 1; i++) {
        if (dfluid.BoundaryCondition[i] != BC_InnerParticle) continue;
        for (int j = i + 1; j < n; j++) {
            if (dfluid.BoundaryCondition[j] == BC_GhostOrDummy) continue;
            float c = dfluid.CoefficientMatrix[j * n + i] / dfluid.CoefficientMatrix[i * n + i];
            for (int k = i + 1; k < n; k++) {
                dfluid.CoefficientMatrix[j * n + k] -= c * dfluid.CoefficientMatrix[i * n + k];
            }
            dfluid.SourceTerm[j] -= c * dfluid.SourceTerm[i];
        }
    }
    for (int i = n - 1; i >= 0; i--) {
        if (dfluid.BoundaryCondition[i] != BC_InnerParticle) continue;
        float sumOfTerms = 0.0;
        for (int j = i + 1; j < n; j++) {
            if (dfluid.BoundaryCondition[j] == BC_GhostOrDummy) continue;
            sumOfTerms += dfluid.CoefficientMatrix[i * n + j] * dfluid.Pressure[j];
        }
        dfluid.Pressure[i] = (dfluid.SourceTerm[i] - sumOfTerms) / dfluid.CoefficientMatrix[i * n + i];
    }
}


void removeNegativePressure(dFluid& dfluid) 
{
    for (int i = 0; i < dfluid.PNum; i++) {
        if (dfluid.Pressure[i] < 0.0)dfluid.Pressure[i] = 0.0;
    }
}


void setMinimumPressure(dFluid& dfluid) {
    for (int i = 0; i < dfluid.PNum; i++) {
        if (dfluid.ParticleType[i] == PT_Ghost || dfluid.ParticleType[i] == PT_DummyWall)continue;
        dfluid.MinimumPressure[i] = dfluid.Pressure[i];
        for (int j = 0; j < dfluid.PNum; j++) {
            if ((j == i) || (dfluid.ParticleType[j] == PT_Ghost)) continue;
            if (dfluid.ParticleType[j] == PT_DummyWall) continue;
            auto distance2 = glm::distance2(dfluid.Position[j], dfluid.Position[i]);
            if (distance2 >= Re2_forGradient)continue;
            if (dfluid.MinimumPressure[i] > dfluid.Pressure[j]) {
                dfluid.MinimumPressure[i] = dfluid.Pressure[j];
            }
        }
    }
}


void calPressureGradient(dFluid& dfluid)
{
    float a = DIM / N0_forGradient;
    for (int i = 0; i < dfluid.PNum; i++) {
        if (dfluid.ParticleType[i] != PT_Fluid) continue;
        auto gradient = vec3(0.0);
        for (int j = 0; j < dfluid.PNum; j++) {
            if (j == i) continue;
            if (dfluid.ParticleType[j] == PT_Ghost) continue;
            if (dfluid.ParticleType[j] == PT_DummyWall) continue;
            auto distance2 = glm::distance2(dfluid.Position[j], dfluid.Position[i]);
            auto distance = sqrt(distance2);
            if (distance < Re_forGradient) {
                auto w = weight(distance, Re_forGradient);
                auto pij = (dfluid.Pressure[j] - dfluid.MinimumPressure[i]) / distance2;
                gradient += (dfluid.Position[j] - dfluid.Position[i]) * pij * w;
            }
        }
        gradient *= a;
        dfluid.Acceleration[i] = (-1.0) * gradient / FluidDensity;
    }
}


void moveParticleUsingPressureGradient(dFluid& dfluid) 
{
    for (int i = 0; i < dfluid.PNum; i++) {
        if (dfluid.ParticleType[i] == PT_Fluid) {
            dfluid.Velocity[i] += dfluid.Acceleration[i] * DT;

            dfluid.Position[i] += dfluid.Acceleration[i] * DT * DT;
        }
        dfluid.Acceleration[i] = vec3(0.0);
    }
}

}
