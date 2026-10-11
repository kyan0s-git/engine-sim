#ifndef ATG_SIMPLE_2D_CONSTRAINT_SOLVER_UTILITIES_H
#define ATG_SIMPLE_2D_CONSTRAINT_SOLVER_UTILITIES_H

#if defined(_MSC_VER)
#define scs_force_inline __forceinline
#else
#define scs_force_inline inline __attribute__((always_inline))
#endif

namespace atg_scs {
    void freeArray(double *&data);
    void freeArray(int *&data);
} /* atg_scs */

#endif /* ATG_SIMPLE_2D_CONSTRAINT_SOLVER_UTILITIES_H */
