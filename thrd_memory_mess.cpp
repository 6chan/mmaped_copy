#include <iostream>

//#include <string.h>
#include <cstring>

//::open() ::mmap()
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <unistd.h> // ftruncate

//error
#include <cerrno>
#include <system_error>

//math ceil floor
//#include <math.h>
#include <cmath>

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

//benchmark test
#include <chrono>

//#define buffer_size 4096
constexpr uint64_t buffer_size = 4096;

const char * debug = "\033[1;33m[DEBUG]: \033[0m"; // yellow
const char * error = 	"\033[1;31m[FAIL]:\033[0m"; // red
const char * succ = "\033[1;32m[SUCESS]:\033[0m"; // green
const char * start = 	"\033[1;36m[STARTING]:\033[0m"; // cyan

std::mutex thread_locker;
//void calc_offset(::FileOffsets & offset); // calculates the correct offset for aligned reading (makes it faster if its aligned)
void cleanup(const int ifd, const int ofd, void * imf, void * omf, int ifs, int ofs); // closes opened files and mmaped files


struct FileOffsets
{
    uint64_t original = 0;
    uint64_t aligned = 0;
};


void calc_offset(FileOffsets & offset, double(*round_function_pointer)(double),  uint64_t ratio = 512) // aligns a position to file by 512 byte ratio
{
    if(offset.original % (uint64_t)ratio)
        offset.aligned = ((*round_function_pointer)( offset.original / (ratio/1.0))) * ratio;
    else
        offset.aligned = offset.original;
}


void cleanup(const int ifd, const int ofd, void * imf, void * omf, int ifs, int ofs)
{
    ::munmap(imf, ifs);
    ::munmap(omf, ofs);
    ::close(ofd);
    ::close(ifd);
}


int main(int argc, char ** argv)
{
    //BENCHMARK
    auto timer_start = std::chrono::high_resolution_clock::now();
    //

    std::vector<std::thread> io_threads;

    int output_file_descriptor = -1;
    char * mapped_output_file = (char *)MAP_FAILED;

    uint64_t output_file_size = 0;
    
    int input_file_descriptor = -1;
    char * mapped_input_file = (char *)MAP_FAILED;

    char * buffer = nullptr;            

    uint64_t data_written = 0;    
    uint64_t missing_bytes = 0;

    //??
    uint64_t bytes_to_write = 1024;


    ::FileOffsets input_file_pointer_offset;
    ::FileOffsets input_file_size;

    uint64_t data_offset_value = 0;

    if(argc != 4)
    {
        std::cout << error << "(parms) 3 parms required, " << argv[0] << " <file_path> <file_size> <offset = 0>!\n";
        return 1;
    }

    try{input_file_pointer_offset.original = std::stoul(argv[3], 0, 10);}
    catch(...)
    {
        std::cout << error << "Invalid value for offset\n";
        return 1;
    }    
    
    calc_offset(input_file_pointer_offset,&floor);

    std::cout << debug << "Offsetvalue: " << input_file_pointer_offset.aligned;

    //getchar();

    auto file_organizer = [argv](int & file_descriptor, 
                            uint64_t & file_size, 
                            char ** file_maped, 
                            int parameter, 
                            int file_flags, 
                            int file_maped_flags, 
                            int size_to_truncate = 0) -> int
    {
        std::cout << debug << "argv["<<parameter<<"] = " << argv[parameter] << '\n';

        file_descriptor = ::open(argv[parameter], file_flags);
        if(file_descriptor == -1)
        {
            std::cout << error << "(::open) " << std::error_code(errno, std::system_category()).message();
            return 1;
        }

        file_size = ::lseek64(file_descriptor, 0, SEEK_END);
        if(file_size == -1)
        {
            ::close(file_descriptor);
            std::cout << error << "(::lseek)" << std::error_code(errno, std::system_category()).message();
            return 1;
        }

        if(file_size < size_to_truncate && size_to_truncate != 0)
        {
            std::cout << debug << "size to truncate = " << size_to_truncate << '\n';

            if(::ftruncate64(file_descriptor, size_to_truncate) != 0)
            {
                std::cout << error << "(::ftruncate64) " << std::error_code(errno, std::system_category()).message();
                return 1;
            }
            file_size = ::lseek64(file_descriptor, 0, SEEK_END);
            if(file_size == -1)
            {
                ::close(file_descriptor);
                std::cout << error << "(::lseek)" << std::error_code(errno, std::system_category()).message();
                return 1;
            }
        }
    
        *file_maped =  static_cast<char *>(::mmap64(nullptr, file_size, file_maped_flags, MAP_SHARED, file_descriptor, /*offset_value*/0));
        if(file_maped == MAP_FAILED)
        {
            ::close(file_descriptor);
            std::cout << error << "(::mmap)" << std::error_code(errno, std::system_category()).message();
            return 1;
        }
        return 0;
    };

    if(file_organizer(
    input_file_descriptor,
    input_file_size.original,
    &mapped_input_file,
    2,
    O_RDONLY,
    PROT_READ
    )) return 1;
    
    if(file_organizer(
    output_file_descriptor, 
    output_file_size, 
    &mapped_output_file,
    1,
    O_CREAT | O_RDWR,
    PROT_READ | PROT_WRITE,
    (input_file_size.original - input_file_pointer_offset.original)
    )) return 1;

    std::atomic<uint64_t> total_data_written{0};
    uint64_t thread_block_size{0};
    uint64_t input_block_size{0};
    uint8_t number_of_threads{2};

    auto threaded_memory_copy = [&total_data_written, thread_block_size]
    (char * calculated_input_file_pointer,
    char * calculated_output_file_pointer,
    uint64_t thread_id) 
    {
        uint64_t bytes_to_write = buffer_size;
        uint64_t local_data_written = 0;
        uint64_t local_missing_bytes = thread_block_size;

        while(local_data_written < thread_block_size)
        {
            if(local_missing_bytes < buffer_size)
                bytes_to_write = local_missing_bytes;
            
            ::memcpy(calculated_output_file_pointer + local_data_written, calculated_input_file_pointer + local_data_written, bytes_to_write);  

            local_data_written += bytes_to_write;
            local_missing_bytes -= bytes_to_write;
        }

        total_data_written += thread_block_size;
    };

    std::cout << start << "\n--Writting changes--\n";
    std::cout << debug << "mapped_output_file = " << (void *)mapped_output_file << '\n';
    std::cout << debug << "mapped_input_file = " << (void *)mapped_input_file << '\n';
    std::cout << debug << "input_file_size.original = " << input_file_size.original << '\n';
    std::cout << debug << "output_file_size = " << output_file_size << '\n';


    calc_offset(input_file_size,&ceil, 512); // we calculate the aligned value of the file size, use use for thread-block alignement

    char * debug_buffer = new char[512];

    // Arranjar o inicio
    ::memcpy(debug_buffer, mapped_input_file + input_file_pointer_offset.aligned, 512);
    ::memcpy(mapped_output_file, debug_buffer + (input_file_pointer_offset.original - input_file_pointer_offset.aligned), 512 - (input_file_pointer_offset.original - input_file_pointer_offset.aligned));
    total_data_written += 512 - (input_file_pointer_offset.original - input_file_pointer_offset.aligned);

    uint64_t y_value = (input_file_size.original - (input_file_size.aligned - 512));
    ::memcpy(mapped_output_file + (input_file_size.original - input_file_pointer_offset.original - y_value), mapped_input_file + input_file_size.aligned - 512, y_value);

    total_data_written += y_value;

    input_block_size = (input_file_size.original - (y_value + input_file_pointer_offset.aligned + 512));
    thread_block_size = input_block_size / number_of_threads; /*nr de threads*/


    mapped_input_file += (input_file_pointer_offset.aligned + 512);
    mapped_output_file += 512;



    std::cout << debug << "mapped_input_file: " << (void *)mapped_input_file << '\n';
    //
    io_threads.emplace_back(std::thread(threaded_memory_copy, mapped_input_file , mapped_output_file, 0));
    io_threads.emplace_back(std::thread(threaded_memory_copy, mapped_input_file + thread_block_size , mapped_output_file + thread_block_size, 1));

    for(int i = 0; i < number_of_threads; ++i)
        io_threads[i].join();

    std::cout << succ << "\n--End writing--\n";
    mapped_input_file -= (input_file_pointer_offset.aligned + 512); // damos fix ao fix
    mapped_output_file -= 512; // same aqui



    if(::msync(mapped_input_file, input_file_size.original, MS_SYNC) == -1)
    {   
        std::cout << error << "(msync input)" << std::error_code(errno, std::system_category()).message() << '\n';
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size.original,output_file_size);
        return 1;
    }

    if(::msync(mapped_output_file, output_file_size, MS_SYNC) == -1)
    {
        std::cout << error << "(msync output)" << std::error_code(errno, std::system_category()).message() << '\n';
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size.original,output_file_size);
        return 1;
    }

    output_file_size = ::lseek64(output_file_descriptor,0 ,SEEK_END);

    if(output_file_size != input_file_size.original - input_file_pointer_offset.original)
    {
        std::cout << error << "(lseek output)" << std::error_code(errno, std::system_category()).message() << '\n';
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size.original,output_file_size);
        return 1;
    }

    cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size.original,output_file_size);
    delete[] buffer;

    //BENCHMARK
    auto timer_finish = std::chrono::high_resolution_clock::now();
    //  
    
    std::cout << succ << "The programm has finnished.\n";
    std::cout << debug << "Duration: " <<  static_cast<std::chrono::duration<double>>(timer_finish - timer_start).count() << std::endl;

    return 0;
}