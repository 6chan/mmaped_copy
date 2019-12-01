#include <iostream>
#include <string.h>

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
#include <math.h>

//threads
//#include <thread>

//benchmark test
#include <chrono>

//#define buffer_size 4096
constexpr uint64_t buffer_size = 4096;


void calc_offset(uint64_t * offset); // calculates the correct offset for aligned reading (makes it faster if its aligned)
void cleanup(const int ifd, const int ofd, void * imf, void * omf, int ifs, int ofs); // closes opened files and mmaped files



typedef struct 
{
  
    uint64_t low_old_offset = 0;
    uint64_t low_new_offset = 0;
    
    //not implemented yet:
    //uint64_t high_old_offset = 0;
    //uint64_t high_new_offset = 0;
} offset;



    // argv[1] = output file
    // argv[2] = input file
    // argv[3] = offset value


void calc_offset(::offset & offset) // aligns a position to file by 512 byte ratio
{
    if(offset.low_old_offset % (uint64_t)512)
        offset.low_new_offset = (floor( offset.low_old_offset / 512.0)) * 512;
    else
        offset.low_new_offset = offset.low_old_offset;
}


void cleanup(const int ifd, const int ofd, void * imf, void * omf, int ifs, int ofs)
{
    std::cout << "Mr Clean.\n";
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

    int output_file_descriptor = -1;
    char * mapped_output_file = nullptr;

    uint64_t output_file_size = 0;
    
    int input_file_descriptor = -1;
    char * mapped_input_file = nullptr;

    uint64_t input_file_size = 0;

    char * buffer = nullptr;

    uint64_t data_written = 0;    
    uint64_t missing_bytes = 0;
    uint64_t bytes_to_write = 1024;


    ::offset offsets;

    uint64_t data_offset_value = 0;

    if(argc != 4)
    {
        std::cout << "(parms) 3 parms required, " << argv[0] << " <file_path> <file_size> <offset = 0>!\n";
        return 1;
    }


    std::cout << "Hanlding offset.\n";
    try{offsets.low_old_offset = std::stoul(argv[3], 0, 10);}
    catch(...)
    {
        std::cout << "Invalid value for offset\n";
        return 1;
    }    
    
    calc_offset(offsets);

    std::cout << "Handling input.\n";

    input_file_descriptor = ::open(argv[2], O_RDWR);
    if(input_file_descriptor == -1)
    {
        std::cout << "(input open) " << std::error_code(errno, std::system_category()).message();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }

    input_file_size = ::lseek64(input_file_descriptor, 0, SEEK_END);
    if(input_file_size == -1)
    {
        std::cout << "(lseek intput)" << std::error_code(errno, std::system_category()).message();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }

    mapped_input_file =  static_cast<char *>(::mmap64(nullptr, input_file_size, PROT_READ, MAP_SHARED, input_file_descriptor, /*offset_value*/0));
    if(mapped_input_file == MAP_FAILED)
    {
        std::cout << "(mmap input)" << std::error_code(errno, std::system_category()).message();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }

    std::cout << "Handling output.\n";

    output_file_descriptor = ::open(argv[1], O_CREAT | O_RDWR | O_TRUNC);
    if(output_file_descriptor == -1)
    {
        std::cout << "(output open) " << std::error_code(errno, std::system_category()).message();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }

    output_file_size = ::lseek64(output_file_descriptor, 0, SEEK_END);
    if(output_file_size == -1)
    {
        std::cout << "(output lseek) " << std::error_code(errno, std::system_category()).message();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }
    
    if(output_file_size < (input_file_size - offsets.low_old_offset))
    {
        if(::ftruncate64(output_file_descriptor, (input_file_size - offsets.low_old_offset)) != 0)
        {
            std::cout << "(ftruncate) " << std::error_code(errno, std::system_category()).message();
            cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
            return 1;
        }
        output_file_size = (input_file_size - offsets.low_old_offset);
    }

    mapped_output_file =  static_cast<char *>(::mmap64(nullptr, output_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, output_file_descriptor,0));
    if(mapped_output_file == MAP_FAILED)
    {
        std::cout << "(mmap output)" << std::error_code(errno, std::system_category()).message();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }

    std::cout << "Writting changes...\n";


    missing_bytes = input_file_size - offsets.low_old_offset;
    data_offset_value = offsets.low_old_offset - offsets.low_new_offset;


    //DEBUG:
    std::cout << "input file size: " << input_file_size << '\n'; // OK
    std::cout << "output file size: " << output_file_size << '\n'; // OK
    std::cout << "offset: new " << offsets.low_new_offset << "    old " << offsets.low_old_offset  << '\n'; // OK
    std::cout << "data missing " << missing_bytes << '\n'; // OK
    std::cout << "data written: " << data_written << '\n'; // OK
    std::cout << "data offset: " << data_offset_value << '\n'; //OK
    getchar();

    buffer = (char *)new char[buffer_size];
    if(!buffer)
    {
        std::cout << "Failed to allocate buffer.\n";
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }



    auto copymem = [&missing_bytes, 
                    &bytes_to_write, 
                    &buffer, 
                    &mapped_input_file,
                    &mapped_output_file, 
                    &data_written, 
                    &offsets,
                    data_offset_value]() 
    {
            if(missing_bytes < buffer_size)
                bytes_to_write = missing_bytes;

            ::memcpy(buffer, mapped_input_file + data_written + offsets.low_new_offset, bytes_to_write);

            ::memcpy(mapped_output_file + data_written, buffer + data_offset_value, bytes_to_write);

            data_written += bytes_to_write;
            missing_bytes -= bytes_to_write;
    };

    copymem();
    data_offset_value = 0;
    while(data_written < output_file_size) copymem();


    std::cout << "\n--End writing--\n";


    if(::msync(mapped_input_file, input_file_size, MS_SYNC) == -1)
    {   
        std::cout << "(msync output)" << std::error_code(errno, std::system_category()).message();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }

    if(::msync(mapped_output_file, output_file_size, MS_SYNC) == -1)
    {
        std::cout << "(msync output)" << std::error_code(errno, std::system_category()).message();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }

    output_file_size = ::lseek64(output_file_descriptor,0 ,SEEK_END);

    if(output_file_size != input_file_size)
    {
        std::cout << "(lseek output)" << std::error_code(errno, std::system_category()).message();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }

    cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);

    //BENCHMARK
    auto timer_finish = std::chrono::high_resolution_clock::now();
    //  
    
    std::cout << "\nSucess!\n";
    std::cout << "Duration: " <<  static_cast<std::chrono::duration<double>>(timer_finish - timer_start).count() << std::endl;

    return 0;
}