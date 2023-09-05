#include "mkl.h"
#include <cstdio>
#include <cstring>
#include <iostream>
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
        if (i < 0 || i >= static_cast<MKL_INT>(this->m)) {
            throw "CooMatrix::put: index of row is outside range";
        }
        if (j < 0 || j >= static_cast<MKL_INT>(this->m)) {
            throw "CooMatrix::put: index of column is outside range";
        }
        if (this->nnz >= this->nnz_max) {
            throw "CooMatrix::put: max number of items has been exceeded";
        }
        this->indices_i[this->nnz] = i;
        this->indices_j[this->nnz] = j;
        this->values_aij[this->nnz] = aij;
        this->nnz++;
    }
};

std::unique_ptr<CooMatrix> read_matrix_market(const std::string &filename) {
    FILE *f = fopen(filename.c_str(), "r");
    if (f == NULL) {
        std::cout << "filename = '" << filename << "'" << std::endl;
        throw "read_matrix_market: cannot open file";
    }

    const int line_max = 2048;
    char line[line_max];

    if (fgets(line, line_max, f) == NULL) {
        fclose(f);
        throw "read_matrix_market: cannot read any line in the file";
    }

    char mm[24], opt[24], fmt[24], kind[24], sym[24];
    int nread = sscanf(line, "%24s %24s %24s %24s %24s", mm, opt, fmt, kind, sym);
    if (nread != 5) {
        fclose(f);
        throw "read_matrix_market: number of tokens in the header is incorrect";
    }
    if (strncmp(mm, "%%MatrixMarket", 14) != 0) {
        fclose(f);
        throw "read_matrix_market: header must start with %%MatrixMarket";
    }
    if (strncmp(opt, "matrix", 6) != 0) {
        fclose(f);
        throw "read_matrix_market: option must be \"matrix\"";
    }
    if (strncmp(fmt, "coordinate", 10) != 0) {
        fclose(f);
        throw "read_matrix_market: type must be \"coordinate\"";
    }
    if (strncmp(kind, "real", 4) != 0) {
        fclose(f);
        throw "read_matrix_market: number kind must be \"real\"";
    }
    if (strncmp(sym, "general", 7) != 0 && strncmp(sym, "symmetric", 9) != 0) {
        fclose(f);
        throw "read_matrix_market: matrix must be \"general\" or \"symmetric\"";
    }

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
            if (nread != 3) {
                fclose(f);
                throw "read_matrix_market: cannot parse the dimensions (m,n,nnz)";
            }
            coo = CooMatrix::make_new(m, nnz);
            coo->symmetric = symmetric;
            initialized = true;
        } else {
            nread = sscanf(line, "%zu %zu %lg", &i, &j, &x);
            if (nread != 3) {
                fclose(f);
                throw "read_matrix_market: cannot parse the values (i,j,x)";
            }
            if (symmetric) {
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
    MKL_INT *row_pointers = NULL;
    MKL_INT *column_indices = NULL;
    double *values = NULL;
    sparse_matrix_t handle = NULL;

    static std::unique_ptr<CsrMatrix> from(std::unique_ptr<CooMatrix> &coo) {
        // access triplet data
        auto ai = coo->indices_i.data();
        auto aj = coo->indices_j.data();
        auto ax = coo->values_aij.data();

        // create COO MKL matrix
        auto m = coo->m;
        auto nnz = coo->nnz;
        sparse_matrix_t coo_mkl = NULL;
        auto status = mkl_sparse_d_create_coo(&coo_mkl, SPARSE_INDEX_BASE_ZERO, m, m, nnz, ai, aj, ax);
        if (status != SPARSE_STATUS_SUCCESS) {
            throw "Intel MKL failed to create COO matrix";
        }

        // convert COO to CSR
        sparse_matrix_t csr = NULL;
        status = mkl_sparse_convert_csr(coo_mkl, SPARSE_OPERATION_NON_TRANSPOSE, &csr);
        if (status != SPARSE_STATUS_SUCCESS) {
            throw "Intel MKL failed to convert COO matrix to CSR matrix";
        }

        // destroy handle to COO
        mkl_sparse_destroy(coo_mkl);

        // get access to internal arrays
        sparse_index_base_t indexing;
        MKL_INT *pointer_b = NULL; // size = dimension
        MKL_INT *pointer_e = NULL; // size = dimension, last entry == nnz (when zero-based)
        MKL_INT *columns = NULL;   // size = nnz
        double *values = NULL;     // size = nnz

        // NOTE about mkl_sparse_d_export_csr
        // The routine returns pointers to the internal representation
        // and DOES NOT ALLOCATE additional memory.
        // https://www.intel.com/content/www/us/en/docs/onemkl/developer-reference-c/2023-2/mkl-sparse-export-csr.html

        status = mkl_sparse_d_export_csr(csr, &indexing, &m, &m, &pointer_b, &pointer_e, &columns, &values);
        if (status != SPARSE_STATUS_SUCCESS) {
            throw "Intel MKL failed to export csr matrix";
        }

        // compute the row indices vector because we need a CSR3 matrix
        MKL_INT *row_pointers = (MKL_INT *)malloc((m + 1) * sizeof(MKL_INT));
        if (row_pointers == NULL) {
            throw "cannot allocate row_pointers";
        }
        for (MKL_INT i = 0; i < m; i++) {
            row_pointers[i] = pointer_b[i];
        }
        row_pointers[m] = pointer_e[m - 1];

        // final number of non-zero values
        MKL_INT final_nnz = row_pointers[m];

        // results
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
        if (this->handle != NULL) {
            mkl_sparse_destroy(this->handle);
        }
    }
};

int main(int argc, char **argv) try {
    auto matrix = std::string("bfwb62.mtx");
    // auto matrix = std::string("pre2.mtx");

    auto coo = read_matrix_market(matrix);
    auto csr = CsrMatrix::from(coo);
    return 0;

    auto rhs = std::vector<double>(csr->m, 1.0);
    auto x = std::vector<double>(csr->m, 0.0);

    MKL_INT opt = MKL_DSS_MSG_LVL_WARNING + MKL_DSS_TERM_LVL_ERROR + MKL_DSS_ZERO_BASED_INDEXING;
    MKL_INT sym = MKL_DSS_NON_SYMMETRIC;
    if (coo->symmetric) {
        sym = MKL_DSS_SYMMETRIC;
    }
    MKL_INT type = MKL_DSS_INDEFINITE;

    _MKL_DSS_HANDLE_t handle;

    // initialize the solver
    auto error = dss_create(handle, opt);
    if (error != MKL_DSS_SUCCESS) {
        throw "DSS failed when creating the solver handle";
    }

    // define the non-zero structure of the matrix
    error = dss_define_structure(handle, sym, csr->row_pointers, csr->m, csr->m, csr->column_indices, csr->nnz);
    if (error != MKL_DSS_SUCCESS) {
        throw "DSS failed when defining the non-zero structure of the matrix";
    }

    // reorder the matrix
    error = dss_reorder(handle, opt, 0);
    if (error != MKL_DSS_SUCCESS) {
        throw "DSS failed when reordering the matrix";
    }

    // factor the matrix
    error = dss_factor_real(handle, type, csr->values);
    if (error != MKL_DSS_SUCCESS) {
        throw "DSS failed when factorizing the matrix";
    }

    // get the solution vector
    MKL_INT n_rhs = 1;
    error = dss_solve_real(handle, opt, rhs.data(), n_rhs, x.data());
    if (error != MKL_DSS_SUCCESS) {
        throw "DSS failed when solving the system";
    }

    // clear dss data
    dss_delete(handle, opt);

    return 0;

} catch (std::exception &e) {
    std::cout << e.what() << std::endl;
} catch (std::string &msg) {
    std::cout << msg.c_str() << std::endl;
} catch (const char *msg) {
    std::cout << msg << std::endl;
} catch (...) {
    std::cout << "some unknown exception happened" << std::endl;
}
