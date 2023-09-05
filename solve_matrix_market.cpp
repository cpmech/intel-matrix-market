#include "mkl.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

struct CooMatrix {
    MKL_INT m;
    MKL_INT nnz;
    MKL_INT nnz_max;
    std::vector<MKL_INT> indices_i;
    std::vector<MKL_INT> indices_j;
    std::vector<double> values_aij;

    inline static std::unique_ptr<CooMatrix> make_new(MKL_INT m, MKL_INT nnz_max) {
        return std::unique_ptr<CooMatrix>{new CooMatrix{
            m,
            0,
            nnz_max,
            std::vector<MKL_INT>(nnz_max, 0),
            std::vector<MKL_INT>(nnz_max, 0),
            std::vector<double>(nnz_max, 0.0),
        }};
    }

    void put(MKL_INT i, MKL_INT j, double aij) {
        this->indices_i[this->nnz] = i;
        this->indices_j[this->nnz] = j;
        this->values_aij[this->nnz] = aij;
        this->nnz++;
    }
};

std::unique_ptr<CooMatrix> read_matrix_market(const std::string &filename) {
    FILE *f = fopen(filename.c_str(), "r");

    const int line_max = 2048;
    char line[line_max];

    fgets(line, line_max, f);

    char mm[24], opt[24], fmt[24], kind[24], sym[24];
    int nread = sscanf(line, "%24s %24s %24s %24s %24s", mm, opt, fmt, kind, sym);

    std::unique_ptr<CooMatrix> coo;
    bool symmetric = strncmp(sym, "symmetric", 9) == 0;

    bool initialized = false;
    size_t m, n, nnz, i, j;
    double x;

    while (fgets(line, line_max, f) != NULL) {
        if (!initialized) {
            if (line[0] == '%') {
                continue;
            }
            nread = sscanf(line, "%zu %zu %zu", &m, &n, &nnz);
            coo = CooMatrix::make_new(m, nnz);
            initialized = true;
        } else {
            nread = sscanf(line, "%zu %zu %lg", &i, &j, &x);
            // must swap lower triangle to upper triangle
            // because MatrixMarket is lower triangle and
            // intel DSS requires upper triangle
            coo->put(j - 1, i - 1, x);
        }
    }
    fclose(f);
    return coo;
}

int main(int argc, char **argv) {
    return 0;
}