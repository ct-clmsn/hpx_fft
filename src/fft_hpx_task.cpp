
#include <hpx/config.hpp>

#if !defined(HPX_COMPUTE_DEVICE_CODE)
#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>
#include <hpx/modules/collectives.hpp>
#include <hpx/iostream.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <fftw3.h>

// use typedef later
using real = double;
using vector_1d = std::vector<real, std::allocator<real>>;
using vector_2d = std::vector<vector_1d>;


struct fft_server : hpx::components::component_base<fft_server>
{

    using fft_backend_plan = fftw_plan;

    public:

        fft_server() = default;

        void initialize(vector_2d values_vec, const std::string COMM_FLAG);
        HPX_DEFINE_COMPONENT_ACTION(fft_server, initialize, initialize_action)

        
        vector_2d fft_2d_r2c();
        HPX_DEFINE_COMPONENT_ACTION(fft_server, fft_2d_r2c, fft_2d_r2c_action)

        virtual ~fft_server()
        {
            fftw_destroy_plan(plan_1d_r2c_);
            fftw_destroy_plan(plan_1d_c2c_);
            fftw_cleanup();
        }

    private:

        void communicate_scatter_r2c(const std::size_t i)
        {
            if(this_locality_ != i)
            {
                // receive from other locality
                communication_vec_[i] = hpx::collectives::scatter_from<vector_1d>(communicators_[i], 
                        hpx::collectives::generation_arg(generation_counter_)).get();
            }
            else
            {
                // send from this locality
                communication_vec_[i] = hpx::collectives::scatter_to(communicators_[i], 
                        std::move(values_prep_), 
                        hpx::collectives::generation_arg(generation_counter_)).get();
            }
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, communicate_scatter_r2c, communicate_scatter_r2c_action)

        void fft_1d_r2c_inplace(const std::size_t i)
        {
            fftw_execute_dft_r2c(plan_1d_r2c_, values_vec_[i].data(), reinterpret_cast<fftw_complex*>(values_vec_[i].data()));
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, fft_1d_r2c_inplace, fft_1d_r2c_inplace_action)

        void split_r2c(const std::size_t i)
        {
            for (std::size_t j = 0; j < num_localities_; ++j) 
            {
                std::move(values_vec_[i].begin() + j * dim_c_y_part_, values_vec_[i].begin() + (j+1) * dim_c_y_part_, values_prep_[j].begin() + i * dim_c_y_part_);
            }
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, split_r2c, split_r2c_action)

        void transpose_r2c(const std::size_t k, const std::size_t i)
        {
            std::uint32_t index_in;
            std::uint32_t index_out;
            const std::uint32_t offset_in = 2 * k;
            const std::uint32_t offset_out = 2 * i;
            const std::uint32_t factor_in = dim_c_y_part_;
            const std::uint32_t factor_out = 2 * num_localities_;
            const std::uint32_t dim_input = communication_vec_[i].size() / factor_in;

            for(std::uint32_t j = 0; j < dim_input; ++j)
            {
                // compute indices once use twice
                index_in = factor_in * j + offset_in;
                index_out = factor_out * j + offset_out;
                // transpose
                trans_values_vec_[k][index_out]     = communication_vec_[i][index_in];
                trans_values_vec_[k][index_out + 1] = communication_vec_[i][index_in + 1];
            }
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, transpose_r2c, transpose_r2c_action)

        void fft_1d_c2c_inplace(const std::size_t i)
        {
            fftw_execute_dft(plan_1d_c2c_, reinterpret_cast<fftw_complex*>(trans_values_vec_[i].data()), reinterpret_cast<fftw_complex*>(trans_values_vec_[i].data()));
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, fft_1d_c2c_inplace, fft_1d_c2c_inplace_action)
   
        void split_c2c(const std::size_t i)
        {
            for (std::size_t j = 0; j < num_localities_; ++j) 
            {
                std::move(trans_values_vec_[i].begin() + j * dim_c_x_part_, trans_values_vec_[i].begin() + (j+1) * dim_c_x_part_, trans_values_prep_[j].begin() + i * dim_c_x_part_);
            }
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, split_c2c, split_c2c_action)

        void transpose_c2c(const std::size_t k, const std::size_t i)
        {
            std::uint32_t index_in;
            std::uint32_t index_out;
            const std::uint32_t offset_in = 2 * k;
            const std::uint32_t offset_out = 2 * i;
            const std::uint32_t factor_in = dim_c_x_part_;
            const std::uint32_t factor_out = 2 * num_localities_;
            const std::uint32_t dim_input = communication_vec_[i].size() / factor_in;

            for(std::uint32_t j = 0; j < dim_input; ++j)
            {
                // compute indices once use twice
                index_in = factor_in * j + offset_in;
                index_out = factor_out * j + offset_out;
                // transpose
                values_vec_[k][index_out]     = communication_vec_[i][index_in];
                values_vec_[k][index_out + 1] = communication_vec_[i][index_in + 1];
            }
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, transpose_c2c, transpose_c2c_action)

        void communicate_scatter_c2c(const std::size_t i)
        {
            if(this_locality_ != i)
            {
                // receive from other locality
                communication_vec_[i] = hpx::collectives::scatter_from<vector_1d>(communicators_[i], 
                        hpx::collectives::generation_arg(generation_counter_)).get();
            }
            else
            {
                // send from this locality
                communication_vec_[i] = hpx::collectives::scatter_to(communicators_[i], 
                        std::move(trans_values_prep_), 
                        hpx::collectives::generation_arg(generation_counter_)).get();
            }
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, communicate_scatter_c2c, communicate_scatter_c2c_action)

        void communicate_all_to_all_c2c()
        {
            communication_vec_ = hpx::collectives::all_to_all(communicators_[0], std::move(trans_values_prep_), hpx::collectives::generation_arg(generation_counter_)).get();
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, communicate_all_to_all_c2c, communicate_all_to_all_c2c_action)

        void communicate_all_to_all_r2c()
        {
            communication_vec_ = hpx::collectives::all_to_all(communicators_[0], std::move(values_prep_), hpx::collectives::generation_arg(generation_counter_)).get();
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, communicate_all_to_all_r2c, communicate_all_to_all_r2c_action)

   
    private:
        unsigned PLAN_FLAG = FFTW_ESTIMATE;
        // locality information
        std::size_t this_locality_ , num_localities_;
        // constants
        std::size_t n_x_local_, n_y_local_;
        std::size_t dim_r_y_, dim_c_y_, dim_c_x_;
        std::size_t dim_c_y_part_, dim_c_x_part_;
        // FFTW plans
        fft_backend_plan plan_1d_r2c_;
        fft_backend_plan plan_1d_c2c_;
        // variables
        std::size_t generation_counter_ = 0;
        // value vectors
        vector_2d values_vec_;
        vector_2d trans_values_vec_;
        vector_2d values_prep_;
        vector_2d trans_values_prep_;
        vector_2d communication_vec_;  
        // future vector
        std::vector<hpx::future<void>> r2c_futures_;
        std::vector<hpx::future<void>> split_x_futures_; 
        std::vector<hpx::shared_future<void>> communication_futures_;
        std::vector<std::vector<hpx::future<void>>> comm_x_futures_;  
        std::vector<hpx::future<void>> c2c_futures_;
        std::vector<hpx::future<void>> split_y_futures_; 
        std::vector<std::vector<hpx::future<void>>> comm_y_futures_;
        // communicators
        std::string COMM_FLAG_;
        std::vector<const char*> basenames_;
        std::vector<hpx::collectives::communicator> communicators_;
};

// The macros below are necessary to generate the code required for exposing
// our partition type remotely.
//
// HPX_REGISTER_COMPONENT() exposes the component creation
// through hpx::new_<>().
typedef hpx::components::component<fft_server> fft_server_type;
HPX_REGISTER_COMPONENT(fft_server_type, fft_server)

// HPX_REGISTER_ACTION() exposes the component member function for remote
// invocation.

typedef fft_server::fft_2d_r2c_action fft_2d_r2c_action;
HPX_REGISTER_ACTION(fft_2d_r2c_action)

typedef fft_server::initialize_action initialize_action;
HPX_REGISTER_ACTION(initialize_action)

///////////////////////////////////////////////////////////////////////////////
// This is a client side member function can now be implemented as the
// fft_server has been defined.
struct fft : hpx::components::client_base<fft, fft_server>
{
    typedef hpx::components::client_base<fft, fft_server> base_type;

    explicit fft()
      : base_type(hpx::new_<fft_server>(hpx::find_here()))
    {
    }

    hpx::future<vector_2d> fft_2d_r2c()
    {
        return hpx::async(hpx::annotated_function(fft_2d_r2c_action(), "total"), get_id());
    }

    hpx::future<void> initialize(vector_2d values_vec, const std::string COMM_FLAG)
    {
        return hpx::async(hpx::annotated_function(initialize_action(), "initialization"), get_id(), std::move(values_vec), COMM_FLAG);
    }

    ~fft() = default;
};

vector_2d fft_server::fft_2d_r2c()
{
    ////////////////////////////////
    // FFTW 1d r2c in first dimension
    for(std::uint32_t i = 0; i < n_x_local_; ++i)
    {
        r2c_futures_[i] = hpx::async(hpx::annotated_function(fft_1d_r2c_inplace_action(), "fft_r2c"), get_id(), i);
        split_x_futures_[i] = r2c_futures_[i].then(
            [=](hpx::future<void> r)
            {
                r.get();
                return hpx::async(hpx::annotated_function(split_r2c_action(), "split_r2c"), get_id(), i);
            }); 
    }
    // local synchronization step for communication
    hpx::shared_future<std::vector<hpx::future<void>>> all_split_x_futures = hpx::when_all(split_x_futures_);
    /////////////////////////////////
    // Communication to get data for FFt in second dimension
    ++generation_counter_;
    if (COMM_FLAG_ == "scatter")
    {
        for (std::size_t i = 0; i < num_localities_; ++i) 
        {
            communication_futures_[i] = all_split_x_futures.then(
                [=](hpx::shared_future<std::vector<hpx::future<void>>> r)
                {
                    r.get();
                    return hpx::async(hpx::annotated_function(communicate_scatter_r2c_action(), "communicate_r2c"), get_id(), i);
                });
        }
        /////////////////////////////////
        // Local tranpose for cohesive memory
        for(std::size_t k = 0; k < n_y_local_; ++k)
        {
            for(std::size_t i = 0; i < num_localities_; ++i)
            {          
                comm_x_futures_[k][i] = communication_futures_[i].then(
                        [=](hpx::shared_future<void> r)
                        {
                            r.get();
                            return hpx::async(hpx::annotated_function(transpose_r2c_action(), "transpose_r2c"), get_id(), k, i);
                        });
            }
        }
    }
    else if (COMM_FLAG_ == "all_to_all")
    {
        communication_futures_[0] = all_split_x_futures.then(
            [=](hpx::shared_future<std::vector<hpx::future<void>>> r)
            {
                r.get();
                return hpx::async(hpx::annotated_function(communicate_all_to_all_r2c_action(), "communicate_r2c"), get_id());
            });
        /////////////////////////////////
        // Local tranpose for cohesive memory
        for(std::size_t k = 0; k < n_y_local_; ++k)
        {
            for(std::size_t i = 0; i < num_localities_; ++i)
            {          
                comm_x_futures_[k][i] = communication_futures_[0].then(
                        [=](hpx::shared_future<void> r)
                        {
                            r.get();
                            return hpx::async(hpx::annotated_function(transpose_r2c_action(), "transpose_r2c"), get_id(), k, i);
                        });
            }
        }
    }
    else
    {
        std::cout << "Communication scheme not specified during initialization\n";
        hpx::finalize();
    }
    /////////////////////////////////
    // FFTW 1d c2c in second dimension
    for(std::size_t i = 0; i < n_y_local_; ++i)
    {
        // synchronize for 1d FFT
        hpx::future<std::vector<hpx::future<void>>> all_comm_x_i_futures = hpx::when_all(comm_x_futures_[i]);
        c2c_futures_[i] = hpx::when_all(all_comm_x_i_futures).then(
            [=](hpx::future<void> r)
            {
                r.get();
                return hpx::async(hpx::annotated_function(fft_1d_c2c_inplace_action(), "fft_c2c"), get_id(), i);
            });     
        split_y_futures_[i] = c2c_futures_[i].then(
            [=](hpx::future<void> r)
            {
                r.get();
                return hpx::async(hpx::annotated_function(split_c2c_action(), "split_c2c"), get_id(), i);
            }); 
    }
    // local synchronization step for communication
    hpx::shared_future<std::vector<hpx::future<void>>> all_split_y_futures = hpx::when_all(split_y_futures_);
    all_split_y_futures.get();// why required?!
    /////////////////////////////////
    // Communication to get original data layout
    ++generation_counter_;
    if (COMM_FLAG_ == "scatter")
    {
        for (std::size_t i = 0; i < num_localities_; ++i) 
        {
            communication_futures_[i] = all_split_y_futures.then(
                [=](hpx::shared_future<std::vector<hpx::future<void>>> r)
                {
                    r.get();
                    return hpx::async(hpx::annotated_function(communicate_scatter_c2c_action(), "communicate_c2c"), get_id(), i);
                });
        }
        /////////////////////////////////
        // Local tranpose back to original dimension
        for(std::size_t k = 0; k < n_x_local_; ++k)
        {
            for(std::size_t i = 0; i < num_localities_; ++i)
            {          
                comm_y_futures_[k][i] = communication_futures_[i].then(
                    [=](hpx::shared_future<void> r)
                    {
                        r.get();
                        return hpx::async(hpx::annotated_function(transpose_c2c_action(), "transpose_c2c"), get_id(), k, i);
                    });
            }
        } 
    }
    else if (COMM_FLAG_ == "all_to_all")
    {
        communication_futures_[0] = all_split_y_futures.then(
            [=](hpx::shared_future<std::vector<hpx::future<void>>> r)
            {
                r.get();
                return hpx::async(hpx::annotated_function(communicate_all_to_all_c2c_action(), "communicate_c2c"), get_id());
            });
        /////////////////////////////////
        // Local tranpose back to original dimension
        for(std::size_t k = 0; k < n_x_local_; ++k)
        {
            for(std::size_t i = 0; i < num_localities_; ++i)
            {          
                comm_y_futures_[k][i] = communication_futures_[0].then(
                    [=](hpx::shared_future<void> r)
                    {
                        r.get();
                        return hpx::async(hpx::annotated_function(transpose_c2c_action(), "transpose_c2c"), get_id(), k, i);
                    });
            }
        } 
    }
    else
    {
        std::cout << "Communication scheme not specified during initialization\n";
        hpx::finalize();
    }
    // wait for every task to finish
    for(std::size_t i = 0; i < n_x_local_; ++i)
    {
        hpx::wait_all(comm_y_futures_[i]);
    };
    return std::move(values_vec_);
}

void fft_server::initialize(vector_2d values_vec, const std::string COMM_FLAG)
{
    // move data into own structure
    values_vec_ = std::move(values_vec);
    // parameters
    this_locality_ = hpx::get_locality_id();
    num_localities_ = hpx::get_num_localities(hpx::launch::sync);
    n_x_local_ = values_vec_.size();
    dim_c_x_ = n_x_local_ * num_localities_;
    dim_c_y_ = values_vec_[0].size() / 2;
    dim_r_y_ = 2 * dim_c_y_ - 2;
    n_y_local_ = dim_c_y_ / num_localities_;
    dim_c_y_part_ = 2 * dim_c_y_ / num_localities_;
    dim_c_x_part_ = 2 * dim_c_x_ / num_localities_;
    // resize other data structures
    trans_values_vec_.resize(n_y_local_);
    values_prep_.resize(num_localities_);
    trans_values_prep_.resize(num_localities_);
    for(std::size_t i = 0; i < n_y_local_; ++i)
    {
        trans_values_vec_[i].resize(2 * dim_c_x_);
    }
    for(std::size_t i = 0; i < num_localities_; ++i)
    {
        values_prep_[i].resize(n_x_local_ * dim_c_y_part_);
        trans_values_prep_[i].resize(n_y_local_ * dim_c_x_part_);
    }
    //create fftw plans
    // forward step one: r2c in y-direction
    plan_1d_r2c_ = fftw_plan_dft_r2c_1d(dim_r_y_,
                                       values_vec_[0].data(),
                                       reinterpret_cast<fftw_complex*>(values_vec_[0].data()),
                                       PLAN_FLAG);
    // forward step two: c2c in x-direction
    plan_1d_c2c_ = fftw_plan_dft_1d(dim_c_x_, 
                                   reinterpret_cast<fftw_complex*>(trans_values_vec_[0].data()), 
                                   reinterpret_cast<fftw_complex*>(trans_values_vec_[0].data()), 
                                   FFTW_FORWARD,
                                   PLAN_FLAG);
    // resize futures
    r2c_futures_.resize(n_x_local_);
    split_x_futures_.resize(n_x_local_);
    c2c_futures_.resize(n_y_local_);
    split_y_futures_.resize(n_y_local_);
    comm_x_futures_.resize(n_y_local_);
    comm_y_futures_.resize(n_x_local_);
    for(std::size_t i = 0; i < n_y_local_; ++i)
    {
        comm_x_futures_[i].resize(num_localities_);
    }
    for(std::size_t i = 0; i < n_x_local_; ++i)
    {
        comm_y_futures_[i].resize(num_localities_);
    }
    // communication specific initialization
    COMM_FLAG_ = COMM_FLAG;
    if (COMM_FLAG_ == "scatter")
    {
        communication_vec_.resize(num_localities_);
        communication_futures_.resize(num_localities_);
        // setup communicators
        basenames_.resize(num_localities_);
        communicators_.resize(num_localities_);
        for(std::size_t i = 0; i < num_localities_; ++i)
        {
            basenames_[i] = std::move(std::to_string(i).c_str());
            communicators_[i] = std::move(hpx::collectives::create_communicator(basenames_[i],
                hpx::collectives::num_sites_arg(num_localities_), hpx::collectives::this_site_arg(this_locality_)));
        }
    }
    else if (COMM_FLAG_ == "all_to_all")
    {
        communication_vec_.resize(1);
        communication_futures_.resize(1);
        // setup communicators
        basenames_.resize(1);
        communicators_.resize(1);
        basenames_[0] = std::move(std::to_string(0).c_str());
        communicators_[0] = std::move(hpx::collectives::create_communicator(basenames_[0],
            hpx::collectives::num_sites_arg(num_localities_), hpx::collectives::this_site_arg(this_locality_)));
    }
    else
    {
        std::cout << "Specify communication scheme: scatter or all_to_all\n";
        hpx::finalize();
    }
}


int hpx_main(hpx::program_options::variables_map& vm)
{
     ////////////////////////////////////////////////////////////////
    // Parameters and Data structures
    // hpx parameters
    const std::uint32_t num_localities = hpx::get_num_localities(hpx::launch::sync);
    //std::uint32_t generation_counter = 1;
    // fft dimension parameters
    const std::uint32_t dim_c_x = vm["nx"].as<std::uint32_t>();//N_X; 
    const std::uint32_t dim_r_y = vm["ny"].as<std::uint32_t>();//N_Y;
    const std::uint32_t dim_c_y = dim_r_y / 2 + 1;
    // division parameters
    const std::uint32_t n_x_local = dim_c_x / num_localities;
    // value vector
    vector_2d values_vec(n_x_local);
    // FFTW plans
    std::string plan_flag = vm["plan"].as<std::string>();
    unsigned FFT_BACKEND_PLAN_FLAG = FFTW_ESTIMATE;
    if( plan_flag == "measure" )
    {
        FFT_BACKEND_PLAN_FLAG = FFTW_MEASURE;
    }
    else if ( plan_flag == "patient")
    {
        FFT_BACKEND_PLAN_FLAG = FFTW_PATIENT;
    }
    else if ( plan_flag == "exhaustive")
    {
        FFT_BACKEND_PLAN_FLAG = FFTW_EXHAUSTIVE;
    }
    //
    std::string run_flag = vm["run"].as<std::string>();
    bool print_result = vm["result"].as<bool>();

    ////////////////////////////////////////////////////////////////
    // initialize values
    for(std::size_t i = 0; i < n_x_local; ++i)
    {
        values_vec[i].resize(2*dim_c_y);
        std::iota(values_vec[i].begin(), values_vec[i].end() - 2, 0.0);
    }
    ////////////////////////////////////////////////////////////////
    // computation   
    // create object: deleted when out of scope
    fft test;
    auto re = test.initialize(std::move(values_vec), run_flag);
    hpx::future<vector_2d> result = re.then(
        [&test](hpx::future<void> r)
        {
           r.get();
           return test.fft_2d_r2c();
        });
    vector_2d a = result.get();
    

    ////////////////////////////////////////////////////////////////
    // print results
    if (print_result)
    {
        const std::uint32_t this_locality = hpx::get_locality_id();
        sleep(this_locality);
        std::string msg = "\nAlgorithm {1}\nLocality {2}\n";
        hpx::util::format_to(hpx::cout, msg, 
                        run_flag, this_locality) << std::flush;
        for (auto r5 : a)
        {
            std::string msg = "\n";
            hpx::util::format_to(hpx::cout, msg) << std::flush;
            std::uint32_t counter = 0;
            for (auto v : r5)
            {
                if(counter%2 == 0)
                {
                    std::string msg = "({1} ";
                    hpx::util::format_to(hpx::cout, msg, v) << std::flush;
                }
                else
                {
                    std::string msg = "{1}) ";
                    hpx::util::format_to(hpx::cout, msg, v) << std::flush;
                }
                ++counter;
            }
        }
        std::string msg2 = "\n";
        hpx::util::format_to(hpx::cout, msg2) << std::flush;
    }
    return hpx::finalize();
}

int main(int argc, char* argv[])
{
    using namespace hpx::program_options;

    options_description desc_commandline;
    desc_commandline.add_options()
    ("result", value<bool>()->default_value(0), "print generated results (default: false)")
    ("nx", value<std::uint32_t>()->default_value(8), "Total x dimension")
    ("ny", value<std::uint32_t>()->default_value(14), "Total y dimension")
    ("plan", value<std::string>()->default_value("estimate"), "FFTW plan (default: estimate)")
    ("run",value<std::string>()->default_value("scatter"), "Choose 2d FFT algorithm communication: scatter or all_to_all");

    // Initialize and run HPX, this example requires to run hpx_main on all
    // localities
    std::vector<std::string> const cfg = {"hpx.run_hpx_main!=1"};

    hpx::init_params init_args;
    init_args.desc_cmdline = desc_commandline;
    init_args.cfg = cfg;

    return hpx::init(argc, argv, init_args);
}

#endif