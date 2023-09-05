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
    bool symmetric;
    std::vector<MKL_INT> indices_i;
    std::vector<MKL_INT> indices_j;
    std::vector<double> values_aij;

    inline static std::unique_ptr<CooMatrix> make_new(MKL_INT m, MKL_INT nnz_max) {
        return std::unique_ptr<CooMatrix>{new CooMatrix{
            m,
            0,
            nnz_max,
            false,
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
    coo->symmetric = strncmp(sym, "symmetric", 9) == 0;

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
            if (coo->symmetric) {
                // must swap lower triangle to upper triangle
                // because MatrixMarket is lower triangle and
                // intel DSS requires upper triangle
                coo->put(j - 1, i - 1, x);
            } else {
                coo->put(i - 1, j - 1, x);
            }
        }
    }
    fclose(f);
    return coo;
}

struct CsrMatrix {
    MKL_INT m;
    MKL_INT nnz;
    MKL_INT *row_pointers;
    MKL_INT *column_indices;
    double *values;
    sparse_matrix_t handle;

    static std::unique_ptr<CsrMatrix> from(std::unique_ptr<CooMatrix> &coo) {
        auto ai = coo->indices_i.data();
        auto aj = coo->indices_j.data();
        auto ax = coo->values_aij.data();

        auto m = coo->m;
        auto nnz = coo->nnz;
        sparse_matrix_t coo_mkl;
        mkl_sparse_d_create_coo(&coo_mkl, SPARSE_INDEX_BASE_ZERO, m, m, nnz, ai, aj, ax);

        sparse_matrix_t csr;
        mkl_sparse_convert_csr(coo_mkl, SPARSE_OPERATION_NON_TRANSPOSE, &csr);
        mkl_sparse_destroy(coo_mkl);

        sparse_index_base_t indexing;
        MKL_INT *pointer_b = NULL;
        MKL_INT *pointer_e = NULL;
        MKL_INT *columns = NULL;
        double *values = NULL;

        mkl_sparse_d_export_csr(csr, &indexing, &m, &m, &pointer_b, &pointer_e, &columns, &values);

        MKL_INT *row_pointers = (MKL_INT *)malloc((m + 1) * sizeof(MKL_INT));
        for (MKL_INT i = 0; i < m; i++) {
            row_pointers[i] = pointer_b[i];
        }
        row_pointers[m] = pointer_e[m - 1];
        MKL_INT final_nnz = row_pointers[m];

        return std::unique_ptr<CsrMatrix>{new CsrMatrix{
            m,
            final_nnz,
            row_pointers,
            columns,
            values,
            csr,
        }};
    }

    ~CsrMatrix() {
        if (this->row_pointers != NULL) {
            free(this->row_pointers);
        }
        mkl_sparse_destroy(this->handle);
    }
};

int main(int argc, char **argv) {
    auto matrix = std::string("bfwb62.mtx");
    // auto matrix = std::string("pre2.mtx");

    auto filename = std::string("~/Downloads/matrix-market/") + matrix;
    auto coo = read_matrix_market(filename);
    auto csr = CsrMatrix::from(coo);

    auto rhs = std::vector<double>(csr->m, 1.0);
    auto x = std::vector<double>(csr->m, 0.0);

    MKL_INT opt = MKL_DSS_MSG_LVL_WARNING + MKL_DSS_TERM_LVL_ERROR + MKL_DSS_ZERO_BASED_INDEXING;
    MKL_INT sym = MKL_DSS_NON_SYMMETRIC;
    if (coo->symmetric) {
        sym = MKL_DSS_SYMMETRIC;
    }
    MKL_INT type = MKL_DSS_INDEFINITE;

    _MKL_DSS_HANDLE_t handle;

    dss_create(handle, opt);
    dss_define_structure(handle, sym, csr->row_pointers, csr->m, csr->m, csr->column_indices, csr->nnz);
    dss_reorder(handle, opt, 0);
    dss_factor_real(handle, type, csr->values);
    MKL_INT n_rhs = 1;
    dss_solve_real(handle, opt, rhs.data(), n_rhs, x.data());
    dss_delete(handle, opt);

    return 0;
}