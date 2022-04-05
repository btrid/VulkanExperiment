/*=====================================================================
  mps.c
  (c) Kazuya SHIBATA, Kohei MUROTANI and Seiichi KOSHIZUKA (2014)

   Fluid Simulation Program Based on a Particle Method (the MPS method)
   Last update: May 21, 2014
=======================================================================*/
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <btrlib/DefineMath.h>

#define DIM                  3
#define PARTICLE_DISTANCE    0.075
#define DT                   0.003
#define OUTPUT_INTERVAL      2 

#define ARRAY_SIZE           5000
#define FINISH_TIME          2.0
#define KINEMATIC_VISCOSITY  (1.0E-6)
#define FLUID_DENSITY        1000.0 
#define Gravity dvec3(0.0, -9.8, 0.0);
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

void initializeParticlePositionAndVelocity_for2dim(void);
void initializeParticlePositionAndVelocity_for3dim(void);
void calConstantParameter(void);
void calNZeroAndLambda(void);
double weight(double distance, double re);
void mainLoopOfSimulation(void);
void calGravity(void);
void calViscosity(void);
void moveParticle(void);
void collision(void);
void calPressure(void);
void calNumberDensity(void);
void setBoundaryCondition(void);
void setSourceTerm(void);
void setMatrix(void);
void exceptionalProcessingForBoundaryCondition(void);
void checkBoundaryCondition(void);
void increaseDiagonalTerm(void);
void solveSimultaniousEquationsByGaussEliminationMethod(void);
void removeNegativePressure(void);
void setMinimumPressure(void);
void calPressureGradient(void);
void moveParticleUsingPressureGradient(void);

static dvec3 Acceleration[ARRAY_SIZE];
static int    ParticleType[ARRAY_SIZE];
static dvec3 Position[ARRAY_SIZE];
static dvec3 Velocity[ARRAY_SIZE];
static double Pressure[ARRAY_SIZE];
static double NumberDensity[ARRAY_SIZE];
static int    BoundaryCondition[ARRAY_SIZE];
static double SourceTerm[ARRAY_SIZE];
static int    FlagForCheckingBoundaryCondition[ARRAY_SIZE];
static double CoefficientMatrix[ARRAY_SIZE * ARRAY_SIZE];
static double MinimumPressure[ARRAY_SIZE];
int    FileNumber;
double Time;
int    NumberOfParticles;
double Re_forNumberDensity, Re2_forNumberDensity;
double Re_forGradient, Re2_forGradient;
double Re_forLaplacian, Re2_forLaplacian;
double N0_forNumberDensity;
double N0_forGradient;
double N0_forLaplacian;
double Lambda;
double collisionDistance, collisionDistance2;
double FluidDensity;


void run1() {
    printf("\n*** START PARTICLE-SIMULATION ***\n");
    initializeParticlePositionAndVelocity_for3dim();
    calConstantParameter();
    mainLoopOfSimulation();
    printf("*** END ***\n\n");
}


void initializeParticlePositionAndVelocity_for3dim(void) {
    int iX, iY, iZ;
    int nX, nY, nZ;
    double x, y, z;
    int i = 0;
    int flagOfParticleGeneration;

    nX = (int)(1.0 / PARTICLE_DISTANCE) + 5;
    nY = (int)(0.6 / PARTICLE_DISTANCE) + 5;
    nZ = (int)(0.3 / PARTICLE_DISTANCE) + 5;
    for (iX = -4; iX < nX; iX++) {
        for (iY = -4; iY < nY; iY++) {
            for (iZ = -4; iZ < nZ; iZ++) {
                x = PARTICLE_DISTANCE * iX;
                y = PARTICLE_DISTANCE * iY;
                z = PARTICLE_DISTANCE * iZ;
                flagOfParticleGeneration = OFF;

                /* dummy wall region */
                if ((((x > -4.0 * PARTICLE_DISTANCE + EPS) && (x <= 1.00 + 4.0 * PARTICLE_DISTANCE + EPS)) && ((y > 0.0 - 4.0 * PARTICLE_DISTANCE + EPS) && (y <= 0.6 + EPS))) && ((z > 0.0 - 4.0 * PARTICLE_DISTANCE + EPS) && (z <= 0.3 + 4.0 * PARTICLE_DISTANCE + EPS))) {
                    ParticleType[i] = PT_DummyWall;
                    flagOfParticleGeneration = ON;
                }

                /* wall region */
                if ((((x > -2.0 * PARTICLE_DISTANCE + EPS) && (x <= 1.00 + 2.0 * PARTICLE_DISTANCE + EPS)) && ((y > 0.0 - 2.0 * PARTICLE_DISTANCE + EPS) && (y <= 0.6 + EPS))) && ((z > 0.0 - 2.0 * PARTICLE_DISTANCE + EPS) && (z <= 0.3 + 2.0 * PARTICLE_DISTANCE + EPS))) {
                    ParticleType[i] = PT_Wall;
                    flagOfParticleGeneration = ON;
                }

                /* wall region */
                if ((((x > -4.0 * PARTICLE_DISTANCE + EPS) && (x <= 1.00 + 4.0 * PARTICLE_DISTANCE + EPS)) && ((y > 0.6 - 2.0 * PARTICLE_DISTANCE + EPS) && (y <= 0.6 + EPS))) && ((z > 0.0 - 4.0 * PARTICLE_DISTANCE + EPS) && (z <= 0.3 + 4.0 * PARTICLE_DISTANCE + EPS))) {
                    ParticleType[i] = PT_Wall;
                    flagOfParticleGeneration = ON;
                }

                /* empty region */
                if ((((x > 0.0 + EPS) && (x <= 1.00 + EPS)) && (y > 0.0 + EPS)) && ((z > 0.0 + EPS) && (z <= 0.3 + EPS))) {
                    flagOfParticleGeneration = OFF;
                }

                /* fluid region */
                if ((((x > 0.0 + EPS) && (x <= 0.25 + EPS)) && ((y > 0.0 + EPS) && (y < 0.5 + EPS))) && ((z > 0.0 + EPS) && (z <= 0.3 + EPS))) {
                    ParticleType[i] = PT_Fluid;
                    flagOfParticleGeneration = ON;
                }

                if (flagOfParticleGeneration == ON) {
                    Position[i] = dvec3(x, y, z);
                    i++;
                }
            }
        }
    }
    NumberOfParticles = i;
    for (i = 0; i < NumberOfParticles; i++) { Velocity[i] = dvec3(0.0); }
}


void calConstantParameter(void) {

    Re_forNumberDensity = RADIUS_FOR_NUMBER_DENSITY;
    Re_forGradient = RADIUS_FOR_GRADIENT;
    Re_forLaplacian = RADIUS_FOR_LAPLACIAN;
    Re2_forNumberDensity = Re_forNumberDensity * Re_forNumberDensity;
    Re2_forGradient = Re_forGradient * Re_forGradient;
    Re2_forLaplacian = Re_forLaplacian * Re_forLaplacian;
    calNZeroAndLambda();
    FluidDensity = FLUID_DENSITY;
    collisionDistance = COLLISION_DISTANCE;
    collisionDistance2 = collisionDistance * collisionDistance;
    FileNumber = 0;
    Time = 0.0;
}


void calNZeroAndLambda(void) {
    int iX, iY, iZ;
    int iZ_start, iZ_end;
    double xj, yj, zj, distance, distance2;
    double xi, yi, zi;

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
                xj = PARTICLE_DISTANCE * (double)(iX);
                yj = PARTICLE_DISTANCE * (double)(iY);
                zj = PARTICLE_DISTANCE * (double)(iZ);
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


double weight(double distance, double re) {
    double weightIJ;

    if (distance >= re) {
        weightIJ = 0.0;
    }
    else {
        weightIJ = (re / distance) - 1.0;
    }
    return weightIJ;
}


void mainLoopOfSimulation(void) {
    int iTimeStep = 0;

    while (1) {
        calGravity();
        calViscosity();
        moveParticle();
        collision();
        calPressure();
        calPressureGradient();
        moveParticleUsingPressureGradient();
        iTimeStep++;
        Time += DT;
        if (Time >= FINISH_TIME) { break; }
    }
}


void calGravity(void) {
    int i;

    for (i = 0; i < NumberOfParticles; i++) {
        if (ParticleType[i] == PT_Fluid) {
            Acceleration[i] = Gravity;
        }
        else {
            Acceleration[i] = dvec3(0.);
        }
    }
}


void calViscosity(void) {
    int i, j;
    dvec3 viscosityTerm;
    double distance, distance2;
    double w;
    double a;

    a = (KINEMATIC_VISCOSITY) * (2.0 * DIM) / (N0_forLaplacian * Lambda);
    for (i = 0; i < NumberOfParticles; i++) {
        if (ParticleType[i] != PT_Fluid) continue;
        viscosityTerm = dvec3(0.0);

        for (j = 0; j < NumberOfParticles; j++) {
            if ((j == i) || (ParticleType[j] == PT_Ghost)) continue;
            auto xij = Position[j] - Position[i];
            distance2 = dot(xij, xij);
            distance = sqrt(distance2);
            if (distance < Re_forLaplacian) {
                w = weight(distance, Re_forLaplacian);
                viscosityTerm += (Velocity[j] - Velocity[i]) * w;
            }
        }
        viscosityTerm = viscosityTerm * a;
        Acceleration[i] += viscosityTerm;
    }
}


void moveParticle(void) {
    int i;

    for (i = 0; i < NumberOfParticles; i++) {
        if (ParticleType[i] == PT_Fluid) {
            Velocity[i] += Acceleration[i] * DT;
            Position[i] += Velocity[i] * DT;
        }
        Acceleration[i] = dvec3(0.0);
    }
}


void collision(void) {
    int    i, j;
    dvec3 xij;
    double distance, distance2;
    double forceDT; /* forceDT is the impulse of collision between particles */
    double mi, mj;
    dvec3 velocity;
    double e = COEFFICIENT_OF_RESTITUTION;
    static dvec3 VelocityAfterCollision[ARRAY_SIZE];

    for (i = 0; i < NumberOfParticles; i++) {
        VelocityAfterCollision[i] = Velocity[i];
    }
    for (i = 0; i < NumberOfParticles; i++) {
        if (ParticleType[i] == PT_Fluid) {
            mi = FluidDensity;
            velocity = Velocity[i];
            for (j = 0; j < NumberOfParticles; j++) {
                if ((j == i) || (ParticleType[j] == PT_Ghost)) continue;
                xij = Position[j] - Position[i];
                distance2 = dot(xij,xij);
                if (distance2 < collisionDistance2) {
                    distance = sqrt(distance2);
                    forceDT = dot((velocity - Velocity[j]) * (xij / distance), dvec3(1.f));
                    if (forceDT > 0.0) {
                        mj = FluidDensity;
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
    for (i = 0; i < NumberOfParticles; i++) {
        if (ParticleType[i] == PT_Fluid) {
            Position[i] += (VelocityAfterCollision[i] - Velocity[i]) * DT;
            Velocity[i] = VelocityAfterCollision[i];
        }
    }
}


void calPressure(void) {
    calNumberDensity();
    setBoundaryCondition();
    setSourceTerm();
    setMatrix();
    solveSimultaniousEquationsByGaussEliminationMethod();
    removeNegativePressure();
    setMinimumPressure();
}


void calNumberDensity(void) {
    int    i, j;
    dvec3 xij;
    double distance, distance2;
    double w;

    for (i = 0; i < NumberOfParticles; i++) {
        NumberDensity[i] = 0.0;
        if (ParticleType[i] == PT_Ghost) continue;
        for (j = 0; j < NumberOfParticles; j++) {
            if ((j == i) || (ParticleType[j] == PT_Ghost)) continue;
            xij = Position[j] - Position[i];
            distance2 = dot(xij, xij);
            distance = sqrt(distance2);
            w = weight(distance, Re_forNumberDensity);
            NumberDensity[i] += w;
        }
    }
}


void setBoundaryCondition(void) {
    int i;
    double n0 = N0_forNumberDensity;
    double beta = THRESHOLD_RATIO_OF_NUMBER_DENSITY;

    for (i = 0; i < NumberOfParticles; i++) {
        if (ParticleType[i] == PT_Ghost || ParticleType[i] == PT_DummyWall) {
            BoundaryCondition[i] = BC_GhostOrDummy;
        }
        else if (NumberDensity[i] < beta * n0) {
            BoundaryCondition[i] = BC_SurfaceParticle;
        }
        else {
            BoundaryCondition[i] = BC_InnerParticle;
        }
    }
}


void setSourceTerm(void) {
    int i;
    double n0 = N0_forNumberDensity;
    double gamma = RELAXATION_COEFFICIENT_FOR_PRESSURE;

    for (i = 0; i < NumberOfParticles; i++) {
        SourceTerm[i] = 0.0;
        if (ParticleType[i] == PT_Ghost || ParticleType[i] == PT_DummyWall) continue;
        if (BoundaryCondition[i] == BC_InnerParticle) {
            SourceTerm[i] = gamma * (1.0 / (DT * DT)) * ((NumberDensity[i] - n0) / n0);
        }
        else if (BoundaryCondition[i] == BC_SurfaceParticle) {
            SourceTerm[i] = 0.0;
        }
    }
}


void setMatrix(void) {
    dvec3 xij;
    double distance, distance2;
    double coefficientIJ;
    double n0 = N0_forLaplacian;
    int    i, j;
    double a;
    int n = NumberOfParticles;

    for (i = 0; i < NumberOfParticles; i++) {
        for (j = 0; j < NumberOfParticles; j++) {
            CoefficientMatrix[i * n + j] = 0.0;
        }
    }

    a = 2.0 * DIM / (n0 * Lambda);
    for (i = 0; i < NumberOfParticles; i++) {
        if (BoundaryCondition[i] != BC_InnerParticle) continue;
        for (j = 0; j < NumberOfParticles; j++) {
            if ((j == i) || (BoundaryCondition[j] == BC_GhostOrDummy)) continue;
            xij = Position[j] - Position[i];
            distance2 = dot(xij, xij);
            distance = sqrt(distance2);
            if (distance >= Re_forLaplacian)continue;
            coefficientIJ = a * weight(distance, Re_forLaplacian) / FluidDensity;
            CoefficientMatrix[i * n + j] = (-1.0) * coefficientIJ;
            CoefficientMatrix[i * n + i] += coefficientIJ;
        }
        CoefficientMatrix[i * n + i] += (COMPRESSIBILITY) / (DT * DT);
    }
    exceptionalProcessingForBoundaryCondition();
}


void exceptionalProcessingForBoundaryCondition(void) {
    /* If tere is no Dirichlet boundary condition on the fluid,
       increase the diagonal terms of the matrix for an exception. This allows us to solve the matrix without Dirichlet boundary conditions. */
    checkBoundaryCondition();
    increaseDiagonalTerm();
}


void checkBoundaryCondition(void) {

    for (int i = 0; i < NumberOfParticles; i++) {
        if (BoundaryCondition[i] == BC_GhostOrDummy) {
            FlagForCheckingBoundaryCondition[i] = BC_GhostOrDummy;
        }
        else if (BoundaryCondition[i] == BC_SurfaceParticle) {
            FlagForCheckingBoundaryCondition[i] = DIRICHLET_BOUNDARY_IS_CONNECTED;
        }
        else {
            FlagForCheckingBoundaryCondition[i] = DIRICHLET_BOUNDARY_IS_NOT_CONNECTED;
        }
    }

    int count = 0;
    do {
        for (int i = 0; i < NumberOfParticles; i++) {
            if (FlagForCheckingBoundaryCondition[i] == DIRICHLET_BOUNDARY_IS_CONNECTED) {
                for (int j = 0; j < NumberOfParticles; j++) {
                    if (j == i) continue;
                    if ((ParticleType[j] == PT_Ghost) || (ParticleType[j] == PT_DummyWall)) continue;
                    if (FlagForCheckingBoundaryCondition[j] == DIRICHLET_BOUNDARY_IS_NOT_CONNECTED) {
                        auto pij = Position[j] - Position[i];
                        auto distance2 = dot(pij, pij);
                        if (distance2 >= Re2_forLaplacian)continue;
                        FlagForCheckingBoundaryCondition[j] = DIRICHLET_BOUNDARY_IS_CONNECTED;
                    }
                }
                FlagForCheckingBoundaryCondition[i] = DIRICHLET_BOUNDARY_IS_CHECKED;
                count++;
            }
        }
    } while (count != 0); /* This procedure is repeated until the all fluid or wall particles (which have Dirhchlet boundary condition in the particle group) are in the state of "DIRICHLET_BOUNDARY_IS_CHECKED".*/

    for (int i = 0; i < NumberOfParticles; i++) {
        if (FlagForCheckingBoundaryCondition[i] == DIRICHLET_BOUNDARY_IS_NOT_CONNECTED) {
            fprintf(stderr, "WARNING: There is no dirichlet boundary condition for %d-th particle.\n", i);
        }
    }
}


void increaseDiagonalTerm(void) {
    int n = NumberOfParticles;

    for (int i = 0; i < n; i++) {
        if (FlagForCheckingBoundaryCondition[i] == DIRICHLET_BOUNDARY_IS_NOT_CONNECTED) {
            CoefficientMatrix[i * n + i] = 2.0 * CoefficientMatrix[i * n + i];
        }
    }
}


void solveSimultaniousEquationsByGaussEliminationMethod(void) {
    int    i, j, k;
    double c;
    double sumOfTerms;
    int    n = NumberOfParticles;

    for (i = 0; i < n; i++) {
        Pressure[i] = 0.0;
    }
    for (i = 0; i < n - 1; i++) {
        if (BoundaryCondition[i] != BC_InnerParticle) continue;
        for (j = i + 1; j < n; j++) {
            if (BoundaryCondition[j] == BC_GhostOrDummy) continue;
            c = CoefficientMatrix[j * n + i] / CoefficientMatrix[i * n + i];
            for (k = i + 1; k < n; k++) {
                CoefficientMatrix[j * n + k] -= c * CoefficientMatrix[i * n + k];
            }
            SourceTerm[j] -= c * SourceTerm[i];
        }
    }
    for (i = n - 1; i >= 0; i--) {
        if (BoundaryCondition[i] != BC_InnerParticle) continue;
        sumOfTerms = 0.0;
        for (j = i + 1; j < n; j++) {
            if (BoundaryCondition[j] == BC_GhostOrDummy) continue;
            sumOfTerms += CoefficientMatrix[i * n + j] * Pressure[j];
        }
        Pressure[i] = (SourceTerm[i] - sumOfTerms) / CoefficientMatrix[i * n + i];
    }
}


void removeNegativePressure(void) {
    int i;

    for (i = 0; i < NumberOfParticles; i++) {
        if (Pressure[i] < 0.0)Pressure[i] = 0.0;
    }
}


void setMinimumPressure(void) {
    int i, j;

    for (i = 0; i < NumberOfParticles; i++) {
        if (ParticleType[i] == PT_Ghost || ParticleType[i] == PT_DummyWall)continue;
        MinimumPressure[i] = Pressure[i];
        for (j = 0; j < NumberOfParticles; j++) {
            if ((j == i) || (ParticleType[j] == PT_Ghost)) continue;
            if (ParticleType[j] == PT_DummyWall) continue;
            auto distance2 = glm::distance2(Position[j], Position[i]);
            if (distance2 >= Re2_forGradient)continue;
            if (MinimumPressure[i] > Pressure[j]) {
                MinimumPressure[i] = Pressure[j];
            }
        }
    }
}


void calPressureGradient(void) {
    int    i, j;
    dvec3 gradient;
    double w, pij;
    double a;

    a = DIM / N0_forGradient;
    for (i = 0; i < NumberOfParticles; i++) {
        if (ParticleType[i] != PT_Fluid) continue;
        gradient = dvec3(0.0);
        for (j = 0; j < NumberOfParticles; j++) {
            if (j == i) continue;
            if (ParticleType[j] == PT_Ghost) continue;
            if (ParticleType[j] == PT_DummyWall) continue;
            auto distance2 = glm::distance2(Position[j], Position[i]);
            auto distance = sqrt(distance2);
            if (distance < Re_forGradient) {
                w = weight(distance, Re_forGradient);
                pij = (Pressure[j] - MinimumPressure[i]) / distance2;
                gradient += (Position[j] - Position[i]) * pij * w;
            }
        }
        gradient *= a;
        Acceleration[i] = (-1.0) * gradient / FluidDensity;
    }
}


void moveParticleUsingPressureGradient(void) {
    int i;

    for (i = 0; i < NumberOfParticles; i++) {
        if (ParticleType[i] == PT_Fluid) {
            Velocity[i] += Acceleration[i] * DT;

            Position[i] += Acceleration[i] * DT * DT;
        }
        Acceleration[i] = dvec3(0.0);
    }
}

