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





int main(int argc, char ** argv)
{
    int output_file_descriptor;
    char * mapped_output_file = nullptr;

    char * buffer = nullptr;

    size_t data_written = 0;
    
    size_t missing_bytes = 0;
    size_t bytes_to_write = 1024;

    size_t output_file_size = 0;
    size_t output_file_final_size = 0;
    
    size_t buffer_size = 0;

    if(argc != 3)
    {
        std::cout << "(parms) 2 parms required, " << argv[0] << " <file_path> <file_size>!\n";
        return 1;
    }

    output_file_descriptor = ::open(argv[1], O_CREAT | O_RDWR);
    if(output_file_descriptor == -1)
    {
        std::cout << "(output open) " << std::error_code(errno, std::system_category()).message();
        return 1;
    }

    output_file_size = std::stoul(argv[2], nullptr, 10);
    if(output_file_size == 0)
    {
        std::cout << "(stoul) fail";
        return 1;
    }

     if(ftruncate64(output_file_descriptor, output_file_size) != 0)
    {
        std::cout << "(ftruncate) " << std::error_code(errno, std::system_category()).message();
        return 1;
    }

    //DEBUG:
    std::cout << "desired file size: " << output_file_size << "file descriptor: " << output_file_descriptor << "\n";
    ////////    


    mapped_output_file =  static_cast<char *>(::mmap64(nullptr, output_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, output_file_descriptor,0));
    if(mapped_output_file == MAP_FAILED)
    {
        std::cout << "(mmap output)" << std::error_code(errno, std::system_category()).message();
        return 1;
    }








    std::cout << "buffer data: \n";
    buffer = new char[1024];
    for(int i = 0; i < 1024; i++)
    {
        buffer[i] = (i%2)?('1'):('0');
        std::cout << buffer[i];
    }
    std::cout << "\n--------------------" << std::endl;

    ////////////////////////////////////////////////////////////////
    missing_bytes = output_file_size;
    std::cout << "missing bytes = " << missing_bytes << '\n';
    std::cout << "bytes to write = " << bytes_to_write << '\n';

    for(int i = 0; data_written < output_file_size; ++i)
    {
        //std::cout << i << "\n"; debug
        if(missing_bytes < 1024)
        {
            bytes_to_write = missing_bytes;
        }

        if(::memcpy(mapped_output_file + data_written, buffer, bytes_to_write) == nullptr)
        {
            std::cout << "(memcpy)" << std::error_code(errno, std::system_category()).message();
            return 1;
        }

        data_written += bytes_to_write;
        missing_bytes -= bytes_to_write;
    }
    ////////////////////////////////////////////////////////////////
    std::cout << "\n--End writing--\n";


    output_file_final_size = ::lseek64(output_file_descriptor,0 ,SEEK_END);

    if(output_file_final_size == -1 || output_file_final_size != output_file_size)
    {
        std::cout << "(lseek input)" << std::error_code(errno, std::system_category()).message();
        return 1;
    }


    if(::msync(mapped_output_file, output_file_size, MS_SYNC) == -1)
    {
        std::cout << "(msync)" << std::error_code(errno, std::system_category()).message();
        return 1;
    }

    if(::munmap(mapped_output_file, output_file_size) == -1)
    {
        std::cout << "(munmap)" << std::error_code(errno, std::system_category()).message();
        return 1;
    }

    if(::fsync(output_file_descriptor) == -1)
    {
        std::cout << "(fsync)" << std::error_code(errno, std::system_category()).message();
        return 1;
    }

    ::close(output_file_descriptor);

    //system("echo 1 > /proc/sys/vm/drop_caches");

    std::cout << "\n\noutput file size after all that: " << output_file_size;
    std::cout << "\n\noutput file size final after all that: " << output_file_final_size;
    std::cout << "\nSucess!" << std::endl;

    return 0;
}


