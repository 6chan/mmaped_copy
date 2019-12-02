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

//threads
//#include <thread>

//benchmark test
#include <chrono>

//#define buffer_size 4096
constexpr uint64_t buffer_size = 4096;


//void calc_offset(::FileOffsets & offset); // calculates the correct offset for aligned reading (makes it faster if its aligned)
void cleanup(const int ifd, const int ofd, void * imf, void * omf, int ifs, int ofs); // closes opened files and mmaped files



struct FileOffsets
{

    uint64_t low_old_offset = 0;
    uint64_t low_new_offset = 0;
    
    //not implemented yet:
    //uint64_t high_old_offset = 0;
    //uint64_t high_new_offset = 0;
};

    // argv[1] = output file
    // argv[2] = input file
    // argv[3] = offset value


void calc_offset(FileOffsets & offset) // aligns a position to file by 512 byte ratio
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
    char * mapped_output_file = (char *)MAP_FAILED;

    uint64_t output_file_size = 0;
    
    int input_file_descriptor = -1;
    char * mapped_input_file = (char *)MAP_FAILED;

    uint64_t input_file_size = 0;

    char * buffer = nullptr;            

    uint64_t data_written = 0;    
    uint64_t missing_bytes = 0;
    uint64_t bytes_to_write = 1024;


    ::FileOffsets offsets;

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



    auto file_organizer = [argv](int & file_descriptor, 
                            uint64_t & file_size, 
                            char ** file_maped, 
                            int parameter, 
                            int file_flags, 
                            int file_maped_flags, 
                            int size_to_truncate = 0) -> int
    {
        std::cout << "argv["<<parameter<<"] = " << argv[parameter] << '\n';

        file_descriptor = ::open(argv[parameter], file_flags);
        if(file_descriptor == -1)
        {
            std::cout << "(::open) " << std::error_code(errno, std::system_category()).message();
            return 1;
        }

        file_size = ::lseek64(file_descriptor, 0, SEEK_END);
        if(file_size == -1)
        {
            ::close(file_descriptor);
            std::cout << "(::lseek)" << std::error_code(errno, std::system_category()).message();
            return 1;
        }

        if(file_size < size_to_truncate && size_to_truncate != 0)
        {
            std::cout << "size to truncate = " << size_to_truncate << '\n';

            if(::ftruncate64(file_descriptor, size_to_truncate) != 0)
            {
                std::cout << "(::ftruncate64) " << std::error_code(errno, std::system_category()).message();
                return 1;
            }
            file_size = ::lseek64(file_descriptor, 0, SEEK_END);
            if(file_size == -1)
            {
                ::close(file_descriptor);
                std::cout << "(::lseek)" << std::error_code(errno, std::system_category()).message();
                return 1;
            }
            std::cout << "Size of file truncated = " << file_size << '\n';
            //file_size = size_to_truncate;
        }
    
        *file_maped =  static_cast<char *>(::mmap64(nullptr, file_size, file_maped_flags, MAP_SHARED, file_descriptor, /*offset_value*/0));
        if(file_maped == MAP_FAILED)
        {
            ::close(file_descriptor);
            std::cout << "(::mmap)" << std::error_code(errno, std::system_category()).message();
            return 1;
        }
        return 0;
    };

    if(file_organizer(
    input_file_descriptor,
    input_file_size,
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
    (input_file_size - offsets.low_old_offset)
    )) return 1;


    std::cout << "Writting changes...\n";


    missing_bytes = input_file_size - offsets.low_old_offset;
    data_offset_value = offsets.low_old_offset - offsets.low_new_offset;


    if(mapped_input_file == MAP_FAILED) std::cout << "map failed.\n"; // OK
    if(mapped_output_file == MAP_FAILED) std::cout << "map failed.\n"; //OK

    /*
    getchar();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 0;
    */


    // NO FREE() YET
    buffer = new char[buffer_size];
    if(!buffer)
    {
        std::cout << "Failed to allocate buffer.\n";
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }


    std::cout << "DEGUB:\n";
    std::cout << "input file size: \t" << input_file_size << '\n';
    std::cout << "output file size: \t" << output_file_size << "\n\n\n";
    std::cout << "buffer address = \t" << &buffer << '\n';
    std::cout << "mapped_input address = \t" << &mapped_input_file << '\n';
    std::cout << "data_written = \t" << data_written << '\n';
    std::cout << "offsets.low_new_offset = \t" << offsets.low_new_offset << '\n';

    /*
    for(int k = 0; k < buffer_size; ++k)
    {
        std::cout << buffer[k] << '\n';
    }
    */
    
    /*
    getchar();
    cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
    return 0;
    */

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


            std::cout << "Loads the buffer OK\n\n";


            std::cout << "\n\n\n\n\n\n\nDEBUG:\n";
            std::cout << "bytes_to_write = " << bytes_to_write << '\n';
            std::cout << "data_written = " << data_written << '\n';
            std::cout << "data_offset_value = " << data_offset_value << '\n';



            buffer[0] = 'A';

            ::memcpy(mapped_output_file + data_written, buffer + data_offset_value, bytes_to_write);

            std::cout << "Writes to output OK\n\n";

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

    if(output_file_size != input_file_size - offsets.low_old_offset)
    {
        std::cout << "(lseek output)" << std::error_code(errno, std::system_category()).message();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }

    cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
    delete[] buffer;

    //BENCHMARK
    auto timer_finish = std::chrono::high_resolution_clock::now();
    //  
    
    std::cout << "\nSucess!\n";
    std::cout << "Duration: " <<  static_cast<std::chrono::duration<double>>(timer_finish - timer_start).count() << std::endl;

    return 0;
}