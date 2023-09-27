#include <hpx/config.hpp>

#if !defined(HPX_COMPUTE_DEVICE_CODE)
#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>
#include <hpx/iostream.hpp>
#include <hpx/timing/high_resolution_timer.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <fftw3.h>

typedef double real;
typedef std::vector<real, std::allocator<real>> vector_1d;
typedef std::vector<vector_1d> vector_2d;

struct fft_server : hpx::components::component_base<fft_server>
{
    typedef fftw_plan fft_backend_plan;
    typedef std::vector<hpx::future<void>> vector_future;

    public:
        fft_server() = default;

        void initialize(vector_2d values_vec, const unsigned PLAN_FLAG);
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
        void fft_1d_r2c_inplace(const std::size_t i)
        {
            fftw_execute_dft_r2c(plan_1d_r2c_, 
                                 values_vec_[i].data(), 
                                 reinterpret_cast<fftw_complex*>(values_vec_[i].data()));
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, fft_1d_r2c_inplace, fft_1d_r2c_inplace_action)

        void fft_1d_c2c_inplace(const std::size_t i)
        {
            fftw_execute_dft(plan_1d_c2c_, 
                             reinterpret_cast<fftw_complex*>(trans_values_vec_[i].data()), 
                             reinterpret_cast<fftw_complex*>(trans_values_vec_[i].data()));
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, fft_1d_c2c_inplace, fft_1d_c2c_inplace_action)

        void transpose_shared_y_to_x(const std::size_t index_trans)
        {
            for( std::size_t index = 0; index < dim_c_y_; ++index)
            {
                trans_values_vec_[index][2 * index_trans] = values_vec_[index_trans][2 * index];
                trans_values_vec_[index][2 * index_trans + 1] = values_vec_[index_trans][2 * index + 1];
            }     
        }
        HPX_DEFINE_COMPONENT_ACTION(fft_server, transpose_shared_y_to_x, transpose_shared_y_to_x_action)

        void transpose_shared_x_to_y(const std::size_t index_trans)
        {
            for( std::size_t index = 0; index < dim_c_x_; ++index)
            {
                values_vec_[index][2 * index_trans] = trans_values_vec_[index_trans][2 * index];
                values_vec_[index][2 * index_trans + 1] = trans_values_vec_[index_trans][2 * index + 1];
            }     
        }      
        HPX_DEFINE_COMPONENT_ACTION(fft_server, transpose_shared_x_to_y, transpose_shared_x_to_y_action)
   
    private:
        // parameters
        std::size_t dim_r_y_, dim_c_y_, dim_c_x_;
        // FFTW plans
        unsigned PLAN_FLAG_;
        fft_backend_plan plan_1d_r2c_;
        fft_backend_plan plan_1d_c2c_;
        // value vectors
        vector_2d values_vec_;
        vector_2d trans_values_vec_;
        // future vectors
        vector_future r2c_futures_;
        vector_future trans_y_to_x_futures_; 
        vector_future c2c_futures_;
        vector_future trans_c2c_futures_; 
};

// HPX_REGISTER_COMPONENT() exposes the component creation
// through hpx::new_<>().
typedef hpx::components::component<fft_server> fft_server_type;
HPX_REGISTER_COMPONENT(fft_server_type, fft_server)

// HPX_REGISTER_ACTION() exposes the component member function
typedef fft_server::fft_2d_r2c_action fft_2d_r2c_action;
HPX_REGISTER_ACTION(fft_2d_r2c_action)

typedef fft_server::initialize_action initialize_action;
HPX_REGISTER_ACTION(initialize_action)


///////////////////////////////////////////////////////////////////////////////
// This is a client side member function
struct fft : hpx::components::client_base<fft, fft_server>
{
    typedef hpx::components::client_base<fft, fft_server> base_type;

    explicit fft()
      : base_type(hpx::new_<fft_server>(hpx::find_here()))
    {
    }

    hpx::future<vector_2d> fft_2d_r2c()
    {
        return hpx::async(fft_2d_r2c_action(), get_id());
    }

    hpx::future<void> initialize(vector_2d values_vec, const unsigned PLAN_FLAG)
    {
        return hpx::async(initialize_action(), get_id(), std::move(values_vec), PLAN_FLAG);
    }

    ~fft() = default;
};

vector_2d fft_server::fft_2d_r2c()
{
    // first dimension
    for(std::size_t i = 0; i < dim_c_x_; ++i)
    {
        // 1d FFT r2c in y-direction
        r2c_futures_[i] = hpx::async(fft_1d_r2c_inplace_action(),get_id(), i);
        // transpose from y-direction to x-direction
        trans_y_to_x_futures_[i] = r2c_futures_[i].then(
            [=](hpx::future<void> r)
            {
                r.get();
                return hpx::async(transpose_shared_y_to_x_action(), get_id(), i);
            }); 
    }
    // synchronization step
    hpx::shared_future<vector_future> all_trans_y_to_x_futures = hpx::when_all(trans_y_to_x_futures_);
    all_trans_y_to_x_futures.get(); // around 5% faster
    // second dimension
    for(std::size_t i = 0; i < dim_c_y_; ++i)
    {
        // 1D FFT in x-direction
        c2c_futures_[i] = hpx::async(fft_1d_c2c_inplace_action(), get_id(), i);
        // all_trans_y_to_x_futures.then(
        //     [=](hpx::shared_future<vector_future> r)
        //     {
        //         r.get();
        //         return hpx::async(fft_1d_c2c_inplace_action(), get_id(), i);
        //     });     
        // transpose from x-direction to y-direction
        trans_c2c_futures_[i] = c2c_futures_[i].then(
            [=](hpx::future<void> r)
            {
                r.get();
                return hpx::async(transpose_shared_x_to_y_action(), get_id(), i);
            }); 
    }
    // synchronization step
    hpx::shared_future<vector_future> all_trans_c2c_futures = hpx::when_all(trans_c2c_futures_);
    all_trans_c2c_futures.get();
    return std::move(values_vec_);
}

void fft_server::initialize(vector_2d values_vec, const unsigned PLAN_FLAG)
{
    // move data into own data structure
    values_vec_ = std::move(values_vec);
    // parameters
    dim_c_x_ = values_vec_.size();
    dim_c_y_ = values_vec_[0].size() / 2;
    dim_r_y_ = 2 * dim_c_y_ - 2;
    // resize transposed data structure
    trans_values_vec_.resize(dim_c_y_);
    for(std::size_t i = 0; i < dim_c_y_; ++i)
    {
        trans_values_vec_[i].resize(2 * dim_c_x_);
    }
    //create fftw plans
    PLAN_FLAG_ = PLAN_FLAG;
    // r2c in y-direction
    plan_1d_r2c_ = fftw_plan_dft_r2c_1d(dim_r_y_,
                                       values_vec_[0].data(),
                                       reinterpret_cast<fftw_complex*>(values_vec_[0].data()),
                                       PLAN_FLAG);
    // c2c in x-direction
    plan_1d_c2c_ = fftw_plan_dft_1d(dim_c_x_, 
                                   reinterpret_cast<fftw_complex*>(trans_values_vec_[0].data()), 
                                   reinterpret_cast<fftw_complex*>(trans_values_vec_[0].data()), 
                                   FFTW_FORWARD,
                                   PLAN_FLAG);
    // resize futures
    r2c_futures_.resize(dim_c_x_);
    trans_y_to_x_futures_.resize(dim_c_x_);
    c2c_futures_.resize(dim_c_y_);
    trans_c2c_futures_.resize(dim_c_y_);
}

void print_vector_2d(const vector_2d& input)
{
    const std::string msg = "\n";
    for (auto vec_1d : input)
    {

        hpx::util::format_to(hpx::cout, msg) << std::flush;
        std::size_t counter = 0;
        for (auto element : vec_1d)
        {
            if(counter%2 == 0)
            {
                std::string msg = "({1} ";
                hpx::util::format_to(hpx::cout, msg, element) << std::flush;
            }
            else
            {
                std::string msg = "{1}) ";
                hpx::util::format_to(hpx::cout, msg, element) << std::flush;
            }
            ++counter;
        }
    }
    hpx::util::format_to(hpx::cout, msg) << std::flush;
}

int hpx_main(hpx::program_options::variables_map& vm)
{
    ////////////////////////////////////////////////////////////////
    // Parameters and Data structures
    // hpx parameters
    const std::size_t num_localities = hpx::get_num_localities(hpx::launch::sync);
    if (std::size_t(1) != num_localities)
    {
        std::cout << "Localities " << num_localities << " instead of 1: Abort runtime\n";
        return hpx::finalize();
    }
    const std::string plan_flag = vm["plan"].as<std::string>();
    bool print_result = vm["result"].as<bool>();
    // time measurement
    auto t = hpx::chrono::high_resolution_timer();
    // fft dimension parameters
    const std::size_t dim_c_x = vm["nx"].as<std::size_t>();//N_X; 
    const std::size_t dim_r_y = vm["ny"].as<std::size_t>();//N_Y;
    const std::size_t dim_c_y = dim_r_y / 2 + 1;
    // data vector
    vector_2d values_vec(dim_c_x);
    // FFTW plans
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
    ////////////////////////////////////////////////////////////////
    // initialize values
    for(std::size_t i = 0; i < dim_c_x; ++i)
    {
        values_vec[i].resize(2*dim_c_y);
        std::iota(values_vec[i].begin(), values_vec[i].end() - 2, 0.0);
    }

    ////////////////////////////////////////////////////////////////
    // computation   
    // create and initialize object (deleted when out of scope)
    fft fft_computer;
    auto start_total = t.now();
    hpx::future<void> future_initialize = fft_computer.initialize(std::move(values_vec), 
                                                                  FFT_BACKEND_PLAN_FLAG);
    future_initialize.get();
    auto stop_init = t.now();
    hpx::future<vector_2d> future_result = fft_computer.fft_2d_r2c();
    values_vec = future_result.get();
    auto stop_total = t.now();

    ////////////////////////////////////////////////////////////////
    // print runtimes
    auto total = stop_total - start_total;
    auto init = stop_init - start_total;
    auto fft2d = stop_total - stop_init;
    std::string msg = "\nLocality 0 - shared\nTotal runtime:  {1}\n"
                      "Initialization: {2}\n"
                      "FFT runtime:    {3}\n\n";
    hpx::util::format_to(hpx::cout, msg,  
                        total,
                        init,
                        fft2d) << std::flush;
    // optional: print results 
    if (print_result)
    {
        print_vector_2d(values_vec);
    }
    return hpx::finalize();
}

int main(int argc, char* argv[])
{
    using namespace hpx::program_options;

    options_description desc_commandline;
    desc_commandline.add_options()
    ("result", value<bool>()->default_value(0), "print generated results (default: false)")
    ("nx", value<std::size_t>()->default_value(8), "Total x dimension")
    ("ny", value<std::size_t>()->default_value(14), "Total y dimension")
    ("plan", value<std::string>()->default_value("estimate"), "FFTW plan (default: estimate)");

    hpx::init_params init_args;
    init_args.desc_cmdline = desc_commandline;

    return hpx::init(argc, argv, init_args);
}

#endif