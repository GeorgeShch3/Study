#include "defs.h"
#include <vector>
#include <utility>

typedef unsigned long long ull;

struct FwdMsg {
    vertex_id_t w_global;
    vertex_id_t v_global;
    ull         sigma_v;
};

struct BwdMsg {
    vertex_id_t v_global;
    double      contrib;
};

static MPI_Datatype MPI_FWD_MSG = MPI_DATATYPE_NULL;
static MPI_Datatype MPI_BWD_MSG = MPI_DATATYPE_NULL;
static bool mpi_types_created = false;

static void create_fwd_msg_type(void) {
    const int count = 3;
    int lengths[3] = {1, 1, 1};
    MPI_Aint displacements[3];
    MPI_Datatype types[3] = {MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED_LONG_LONG};
    
    FwdMsg dummy;
    
    MPI_Get_address(&dummy.w_global, &displacements[0]);
    MPI_Get_address(&dummy.v_global, &displacements[1]);
    MPI_Get_address(&dummy.sigma_v,  &displacements[2]);
    
    MPI_Aint base = displacements[0];
    displacements[0] = 0;
    displacements[1] -= base;
    displacements[2] -= base;
    
    MPI_Type_create_struct(count, lengths, displacements, types, &MPI_FWD_MSG);
    MPI_Type_commit(&MPI_FWD_MSG);
}

static void create_bwd_msg_type(void) {
    const int count = 2;
    int lengths[2] = {1, 1};
    MPI_Aint displacements[2];
    MPI_Datatype types[2] = {MPI_UNSIGNED, MPI_DOUBLE};
    
    BwdMsg dummy;
    
    MPI_Get_address(&dummy.v_global, &displacements[0]);
    MPI_Get_address(&dummy.contrib,  &displacements[1]);
    
    MPI_Aint base = displacements[0];
    displacements[0] = 0;
    displacements[1] -= base;
    
    MPI_Type_create_struct(count, lengths, displacements, types, &MPI_BWD_MSG);
    MPI_Type_commit(&MPI_BWD_MSG);
}

static void create_mpi_types(void) {
    if (mpi_types_created) return;
    create_fwd_msg_type();
    create_bwd_msg_type();
    mpi_types_created = true;
}

static void free_mpi_types(void) {
    if (MPI_FWD_MSG != MPI_DATATYPE_NULL) {
        MPI_Type_free(&MPI_FWD_MSG);
        MPI_FWD_MSG = MPI_DATATYPE_NULL;
    }
    if (MPI_BWD_MSG != MPI_DATATYPE_NULL) {
        MPI_Type_free(&MPI_BWD_MSG);
        MPI_BWD_MSG = MPI_DATATYPE_NULL;
    }
    mpi_types_created = false;
}

template<typename T>
static void exchange_bufs(int np, std::vector<std::vector<T>>& sb, std::vector<std::vector<T>>& rb, MPI_Datatype mpi_type, int tag) {
    std::vector<int> scnt(np), rcnt(np);
    for (int p = 0; p < np; ++p)
        scnt[p] = static_cast<int>(sb[p].size());

    MPI_Alltoall(scnt.data(), 1, MPI_INT, rcnt.data(), 1, MPI_INT, MPI_COMM_WORLD);

    for (int p = 0; p < np; ++p)
        rb[p].resize(rcnt[p]);

    std::vector<MPI_Request> reqs;
    reqs.reserve(2 * np);

    for (int p = 0; p < np; ++p) {
        if (rcnt[p] > 0) {
            MPI_Request r;
            MPI_Irecv(rb[p].data(), rcnt[p], mpi_type, p, tag, MPI_COMM_WORLD, &r);
            reqs.push_back(r);
        }
    }

    for (int p = 0; p < np; ++p) {
        if (scnt[p] > 0) {
            MPI_Request r;
            MPI_Isend(sb[p].data(), scnt[p], mpi_type, p, tag, MPI_COMM_WORLD, &r);
            reqs.push_back(r);
        }
    }

    if (!reqs.empty())
        MPI_Waitall(static_cast<int>(reqs.size()), reqs.data(), MPI_STATUSES_IGNORE);
}

static void exchange_fwd(int np, std::vector<std::vector<FwdMsg>>& sb, std::vector<std::vector<FwdMsg>>& rb, int tag) {
    exchange_bufs<FwdMsg>(np, sb, rb, MPI_FWD_MSG, tag);
}

static void exchange_bwd(int np, std::vector<std::vector<BwdMsg>>& sb, std::vector<std::vector<BwdMsg>>& rb, int tag) {
    exchange_bufs<BwdMsg>(np, sb, rb, MPI_BWD_MSG, tag);
}

void run(graph_t *G, double *result)
{
    const int         rank    = G->rank;
    const int         np      = G->nproc;
    const vertex_id_t n       = G->n;
    const vertex_id_t local_n = G->local_n;

    create_mpi_types();

    std::fill(result, result + local_n, 0.0);

    std::vector<unsigned> d(local_n);
    std::vector<ull>      sigma(local_n);
    std::vector<double>   delta(local_n);

    std::vector<std::vector<std::pair<vertex_id_t, ull>>> pred(local_n);

    std::vector<std::vector<vertex_id_t>> level_verts;

    std::vector<std::vector<FwdMsg>> fwd_sb(np), fwd_rb(np);
    std::vector<std::vector<BwdMsg>> bwd_sb(np), bwd_rb(np);

    for (vertex_id_t s_global = 0; s_global < n; ++s_global) {

        std::fill(d.begin(),     d.end(),     UINT32_MAX);
        std::fill(sigma.begin(), sigma.end(), 0ULL);
        std::fill(delta.begin(), delta.end(), 0.0);
        for (auto& pv : pred) pv.clear();
        level_verts.clear();

        const int s_owner = VERTEX_OWNER(s_global, n, np);

        std::vector<vertex_id_t> cur_level;
        if (rank == s_owner) {
            const vertex_id_t sl = VERTEX_LOCAL(s_global, n, np, rank);
            d[sl]     = 0;
            sigma[sl] = 1;
            cur_level.push_back(sl);
        }
        level_verts.push_back(cur_level);

        for (unsigned dist = 0; ; ++dist) {

            for (int p = 0; p < np; ++p) fwd_sb[p].clear();
            std::vector<vertex_id_t> nxt_level;

            for (const vertex_id_t ul : cur_level) {
                const vertex_id_t ug  = VERTEX_TO_GLOBAL(ul, n, np, rank);
                const ull         sig = sigma[ul];

                for (edge_id_t e  = G->rowsIndices[ul]; e  < G->rowsIndices[ul + 1]; ++e) {

                    const vertex_id_t wg    = G->endV[e];
                    const int         w_own = VERTEX_OWNER(wg, n, np);

                    if (w_own == rank) {
                        const vertex_id_t wl = VERTEX_LOCAL(wg, n, np, rank);

                        if (d[wl] == UINT32_MAX) {
                            d[wl]     = dist + 1;
                            sigma[wl] = sig;
                            pred[wl].push_back({ug, sig});
                            nxt_level.push_back(wl);
                        } else if (d[wl] == dist + 1) {
                            sigma[wl] += sig;
                            pred[wl].push_back({ug, sig});
                        }

                    } else {
                        fwd_sb[w_own].push_back({wg, ug, sig});
                    }
                }
            }

            exchange_fwd(np, fwd_sb, fwd_rb, 0);

            for (int p = 0; p < np; ++p) {
                for (const FwdMsg& msg : fwd_rb[p]) {
                    const vertex_id_t wl = VERTEX_LOCAL(msg.w_global, n, np, rank);

                    if (d[wl] == UINT32_MAX) {
                        d[wl]     = dist + 1;
                        sigma[wl] = msg.sigma_v;
                        pred[wl].push_back({msg.v_global, msg.sigma_v});
                        nxt_level.push_back(wl);
                    } else if (d[wl] == dist + 1) {
                        sigma[wl] += msg.sigma_v;
                        pred[wl].push_back({msg.v_global, msg.sigma_v});
                    }
                }
            }

            int loc_sz  = static_cast<int>(nxt_level.size());
            int glob_sz = 0;
            MPI_Allreduce(&loc_sz, &glob_sz, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

            if (glob_sz == 0) break; 

            level_verts.push_back(nxt_level);
            cur_level = std::move(nxt_level);
        }

        for (int l = static_cast<int>(level_verts.size()) - 1; l >= 1; --l) {

            for (int p = 0; p < np; ++p) bwd_sb[p].clear();

            for (const vertex_id_t ul : level_verts[l]) {
                const double su = static_cast<double>(sigma[ul]);
                const double du = delta[ul];

                for (const auto& [vg, sv] : pred[ul]) {
                    const double contrib =
                        (static_cast<double>(sv) / su) * (1.0 + du);

                    const int v_own = VERTEX_OWNER(vg, n, np);

                    if (v_own == rank) {
                        delta[VERTEX_LOCAL(vg, n, np, rank)] += contrib;
                    } else {
                        bwd_sb[v_own].push_back({vg, contrib});
                    }
                }
            }

            exchange_bwd(np, bwd_sb, bwd_rb, 1);

            for (int p = 0; p < np; ++p) {
                for (const BwdMsg& msg : bwd_rb[p]) {
                    delta[VERTEX_LOCAL(msg.v_global, n, np, rank)] += msg.contrib;
                }
            }
        }

        const int sl = (rank == s_owner) ? static_cast<int>(VERTEX_LOCAL(s_global, n, np, rank)) : -1;

        for (vertex_id_t u = 0; u < local_n; ++u)
            if (static_cast<int>(u) != sl)
                result[u] += delta[u];
    }

    for (vertex_id_t i = 0; i < local_n; ++i) result[i] /= 2.0;
}