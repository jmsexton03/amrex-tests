#include <AMReX.H>
#include <AMReX_Gpu.H>
#include <AMReX_Arena.H>
#include <AMReX_Random.H>
#include <AMReX_ParmParse.H>
#include <AMReX_ParallelDescriptor.H>
#include <AMReX_ParallelReduce.H>

// ================================================

#ifdef AMREX_USE_NCCL

#include <nccl.h>

#define NCCLCHECK(cmd) do {                         \
  ncclResult_t r = cmd;                             \
  if (r!= ncclSuccess) {                            \
    printf("Failed, NCCL error %s:%d '%s'\n",       \
        __FILE__,__LINE__,ncclGetErrorString(r));   \
    exit(EXIT_FAILURE);                             \
  }                                                 \
} while(0)

#endif

// ================================================

#ifdef AMREX_USE_NVSHMEM

#include <nvshmem.h>
#include <nvshmemx.h>

// For collective launch:
#define NVSHMEM_CHECK(stmt)                                                                \
do {                                                                                   \
    int result = (stmt);                                                               \
    if (NVSHMEMX_SUCCESS != result) {                                                  \
        fprintf(stderr, "[%s:%d] nvshmem failed with error %d \n", __FILE__, __LINE__, \
                result);                                                               \
        exit(-1);                                                                      \
    }                                                                                  \
} while (0)

#endif

// ================================================

template <class T>
long compare(const long n_ele, double epsilon,
             const T* A, const T* B,
             const std::string& name_A, const std::string& name_B) {

    long errors = 0;

    for (int i=0; i<n_ele; ++i) {
        double diff = std::abs(A[i]-B[i]);

        if ( diff >= epsilon ) {
            errors++;
            amrex::Print() << name_A << "/" << name_B << " #i " << " don't match: "
                           << std::setprecision(17) << A[i] << " " << B[i];
        }
    }

    return errors;
}

using namespace amrex;
void main_main();


int main (int argc, char* argv[])
{
    amrex::Initialize(argc,argv);

    main_main();

    amrex::Finalize();
}

void main_main ()
{
    BL_PROFILE("main");

    int n_warmup, n_tests, check, do_aware;
    int min_elements, max_elements, factor;
    Real epsilon;
    {
        ParmParse pp;
        pp.get("warmup_count", n_warmup);
        pp.get("test_count", n_tests);

        pp.get("min_elements", min_elements);
        pp.get("max_elements", max_elements);
        pp.get("mult_factor", factor);

        pp.get("check_result", check);
        pp.get("epsilon", epsilon);
        pp.get("do_cuda_aware_mpi", do_aware);
    }

//  amrex::ResetRandomSeed(27182182459045);

    MPI_Comm comm = ParallelDescriptor::Communicator();

// ***************************************************************

#ifdef AMREX_USE_NCCL

    // NCCL Comm Setup
    ncclComm_t nccl_comm;
    {
        ncclUniqueId id;

        int nRanks = ParallelDescriptor::NProcs();
        int myProc = ParallelDescriptor::MyProc();

        // get NCCL unique ID at rank 0 and broadcast it to all others
        if (myProc == 0) NCCLCHECK(ncclGetUniqueId(&id));
        ParallelDescriptor::Bcast((char*) (&id), sizeof(id));

        // initializing NCCL
        NCCLCHECK(ncclCommInitRank(&nccl_comm, nRanks, id, myProc));
    }

    ncclDataType_t NCCLTYPE;
    if (sizeof(Real) == sizeof(float)) {
        NCCLTYPE = ncclFloat;
    } else if (sizeof(Real) == sizeof(double)) {
        NCCLTYPE = ncclDouble;
    }

#endif

#ifdef AMREX_USE_NVSHMEM

    amrex::Print() << "Start NVSHMEM Init" << std::endl;

    nvshmemx_init_attr_t attr;
    attr.mpi_comm = &comm;
    nvshmemx_init_attr(NVSHMEMX_INIT_WITH_MPI_COMM, &attr);

    amrex::Print() << "Finish NVSHMEM Init" << std::endl;

#endif

    // NCCL API calls:
    // Allreduce, Bcast, Reduce, Allgather, ReduceScatter

    for (int n_ele=min_elements; n_ele<=max_elements; n_ele*=factor) {

        BL_PROFILE_REGION("Test Size = " + std::to_string(n_ele));

        size_t sz = sizeof(Real)*n_ele;

        void* c_buff = The_Cpu_Arena()->alloc(sz);
        void* p_buff = The_Pinned_Arena()->alloc(sz);

        void* d1_buff = The_Device_Arena()->alloc(sz);
        void* d2_buff = The_Device_Arena()->alloc(sz);
        void* d3_buff = The_Device_Arena()->alloc(sz);
        void* d4_buff = The_Device_Arena()->alloc(sz);
        void* d5_buff = The_Device_Arena()->alloc(sz);
        void* d6_buff = The_Device_Arena()->alloc(sz);

        std::vector<Real> cpu(n_ele, 0);
        std::vector<Real> zero(n_ele, 0);
        std::vector<Real> data(n_ele, 0);
        for (auto& i : zero) { i = 0.0; }
        for (auto& i : data) { i = RandomNormal(1.0, 0.5); }

// ==================================================================================
        BL_PROFILE_VAR_NS("AllReduce::CPU - " + std::to_string(n_ele), cpu_p);
        for (int i=0; i<n_warmup+n_tests; ++i)
        {
            cpu = data;
            if (i >= n_warmup) { BL_PROFILE_VAR_START(cpu_p); }

            amrex::ParallelAllReduce::Sum<Real>(cpu.data(), n_ele, comm);

            if (i >= n_warmup) { BL_PROFILE_VAR_STOP(cpu_p); }
        }
// ==================================================================================
        BL_PROFILE_VAR_NS("AllReduce::GPU - " + std::to_string(n_ele), gpu_p);
        for (int i=0; i<n_warmup+n_tests; ++i)
        {
            Gpu::htod_memcpy(d1_buff, data.data(), sz);

            if (i >= n_warmup) { BL_PROFILE_VAR_START(gpu_p); }

            Gpu::dtoh_memcpy(p_buff, d1_buff, sz);
            amrex::ParallelAllReduce::Sum<Real>(reinterpret_cast<Real*>(p_buff), n_ele, comm);
            Gpu::htod_memcpy(d1_buff, p_buff, sz);

            if (i >= n_warmup) { BL_PROFILE_VAR_STOP(gpu_p); }
        }
// ==================================================================================
        if (do_aware) {
            BL_PROFILE_VAR_NS("AllReduce: GPU Aware - " + std::to_string(n_ele), aware_p);

            for (int i=0; i<n_warmup+n_tests; ++i)
            {
                Gpu::htod_memcpy(d2_buff, data.data(), sz);

                if (i >= n_warmup) { BL_PROFILE_VAR_START(aware_p); }

                amrex::ParallelAllReduce::Sum<Real>(reinterpret_cast<Real*>(d2_buff), n_ele, comm);

                if (i >= n_warmup) { BL_PROFILE_VAR_STOP(aware_p); }
            }
        }
// ==================================================================================
#ifdef AMREX_USE_NCCL
        BL_PROFILE_VAR_NS("AllReduce: NCCL - " + std::to_string(n_ele), nccl_p);
        BL_PROFILE_VAR_NS("AllReduce: NCCL to CPU - " + std::to_string(n_ele), ncclcpu_p);
        Gpu::htod_memcpy(d3_buff, data.data(), sz);

        for (int i=0; i<n_warmup+n_tests; ++i)
        {
            Gpu::htod_memcpy(d4_buff, zero.data(), sz);

            if (i >= n_warmup) { BL_PROFILE_VAR_START(ncclcpu_p); BL_PROFILE_VAR_START(nccl_p); }

            NCCLCHECK( ncclAllReduce(d3_buff, d4_buff, n_ele,
                                     NCCLTYPE, ncclSum, nccl_comm, Gpu::Device::gpuStream()) );

            if (i >= n_warmup) { BL_PROFILE_VAR_STOP(nccl_p); }

            Gpu::dtoh_memcpy(d4_buff, p_buff, sz);

            if (i >= n_warmup) { BL_PROFILE_VAR_STOP(ncclcpu_p); }
        }
#endif
// ==================================================================================
#ifdef AMREX_USE_NVSHMEM
        BL_PROFILE_VAR_NS("AllReduce: NVSHMEM - " + std::to_string(n_ele), nvs_p);
        BL_PROFILE_VAR_NS("AllReduce: NVSHMEM to CPU - " + std::to_string(n_ele), nvscpu_p);
        Gpu::htod_memcpy(d5_buff, data.data(), sz);

        for (int i=0; i<n_warmup+n_tests; ++i)
        {
            Gpu::htod_memcpy(d6_buff, zero.data(), sz);

            if (i >= n_warmup) { BL_PROFILE_VAR_START(nvscpu_p); BL_PROFILE_VAR_START(nvs_p); }

            NVSHMEM_CHECK( nvshmem_double_sum_reduce( NVSHMEM_TEAM_WORLD,
                                                      reinterpret_cast<double*> (d6_buff),
                                                      reinterpret_cast<double*> (d5_buff), sz ) );
            nvshmem_barrier_all();

            if (i >= n_warmup) { BL_PROFILE_VAR_STOP(nvs_p); }

            Gpu::dtoh_memcpy(d6_buff, p_buff, sz);

            if (i >= n_warmup) { BL_PROFILE_VAR_STOP(nvscpu_p); }
        }
#endif
// ==================================================================================

        if (check) {

            // For each, compare to CPU.
            // Collect the data in "answer" and compare item-by-item.

            int wrong = 0;
            std::vector<Real> answer(n_ele);

            Gpu::dtoh_memcpy(c_buff, d1_buff, sz);
            for (int i=0; i<n_ele; ++i)
                { answer[i] = reinterpret_cast<Real*>(c_buff)[i]; }

            wrong += compare(n_ele, epsilon, cpu.data(), answer.data(), "CPU", "GPU");

            if (do_aware) {
                Gpu::dtoh_memcpy(c_buff, d2_buff, sz);
                for (int i=0; i<n_ele; ++i)
                    { answer[i] = reinterpret_cast<Real*>(c_buff)[i]; }

                wrong += compare(n_ele, epsilon, cpu.data(), answer.data(), "CPU", "AwareMPI");
            }

#ifdef USE_NCCL
            Gpu::dtoh_memcpy(c_buff, d4_buff, sz);
            for (int i=0; i<n_ele; ++i)
                { answer[i] = reinterpret_cast<Real*>(c_buff)[i]; }

            wrong += compare(n_ele, epsilon, cpu.data(), answer.data(), "CPU", "NCCL");
#endif

#ifdef USE_NVSHMEM
            Gpu::dtoh_memcpy(c_buff, d6_buff, sz);
            for (int i=0; i<n_ele; ++i)
                { answer[i] = reinterpret_cast<Real*>(c_buff)[i]; }

            wrong += compare(n_ele, epsilon, cpu.data(), answer.data(), "CPU", "NVSHMEM");
#endif

            if (wrong == 0)
                { amrex::Print() << "All reductions match!" << std::endl; }
        }

        The_Cpu_Arena()->free(c_buff);
        The_Pinned_Arena()->free(p_buff);
        The_Device_Arena()->free(d1_buff);
        The_Device_Arena()->free(d2_buff);
        The_Device_Arena()->free(d3_buff);
        The_Device_Arena()->free(d4_buff);
        The_Device_Arena()->free(d5_buff);
        The_Device_Arena()->free(d6_buff);
    }

#ifdef AMREX_USE_NCCL
    NCCLCHECK(ncclCommDestroy(nccl_comm));
#endif

#ifdef AMREX_USE_NVSHMEM
    nvshmem_finalize();
#endif
}
