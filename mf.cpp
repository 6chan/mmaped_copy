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

//threads
#include <thread>




#define BUFFER_SIZE 4096

    // argv[1] = output file
    // argv[2] = input file

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
    int output_file_descriptor = -1;
    char * mapped_output_file = nullptr;

    size_t output_file_size = 0;
    
    int input_file_descriptor = -1;
    char * mapped_input_file = nullptr;

    size_t input_file_size = 0;

    char * buffer = nullptr;

    size_t data_written = 0;    
    size_t missing_bytes = 0;
    size_t bytes_to_write = 1024;


    if(argc != 3)
    {
        std::cout << "(parms) 2 parms required, " << argv[0] << " <file_path> <file_size>!\n";
        return 1;
    }

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

    mapped_input_file =  static_cast<char *>(::mmap64(nullptr, input_file_size, PROT_READ, MAP_SHARED, input_file_descriptor,0));
    if(mapped_input_file == MAP_FAILED)
    {
        std::cout << "(mmap input)" << std::error_code(errno, std::system_category()).message();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }

    std::cout << "Handling output.\n";

    output_file_descriptor = ::open(argv[1], O_CREAT | O_RDWR);
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
    
    if(output_file_size < input_file_size)
    {
        if(::ftruncate64(output_file_descriptor, input_file_size) != 0)
        {
            std::cout << "(ftruncate) " << std::error_code(errno, std::system_category()).message();
            cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
            return 1;
        }
        output_file_size = input_file_size;
    }

    mapped_output_file =  static_cast<char *>(::mmap64(nullptr, output_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, output_file_descriptor,0));
    if(mapped_output_file == MAP_FAILED)
    {
        std::cout << "(mmap output)" << std::error_code(errno, std::system_category()).message();
        cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);
        return 1;
    }

    std::cout << "Writting changes...\n";

    missing_bytes = input_file_size;

    buffer = (char *)new char[BUFFER_SIZE];
    if(!buffer)
    {
        std::cout << "Failed to allocate buffer.\n";
        ::munmap(mapped_input_file, input_file_size);
        ::munmap(mapped_output_file, output_file_size);
        ::close(output_file_descriptor);
        ::close(input_file_descriptor);
        return 1;
    }

    while(data_written < output_file_size)
    {
        if(missing_bytes < BUFFER_SIZE)
            bytes_to_write = missing_bytes;

        ::memcpy(buffer, mapped_input_file + data_written, bytes_to_write);

        ::memcpy(mapped_output_file + data_written, buffer, bytes_to_write);
        
        data_written += bytes_to_write;
        missing_bytes -= bytes_to_write;
    }

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

    /* not worth (?)
    if(::munmap(mapped_output_file, output_file_size) == -1)
    {
        std::cout << "(munmap output)" << std::error_code(errno, std::system_category()).message();
        return 1;
    }
    if(::munmap(mapped_input_file, input_file_size) == -1)
    {
        std::cout << "(munmap input)" << std::error_code(errno, std::system_category()).message();
        cleanup()
        return 1;
    }
    */

    cleanup(input_file_descriptor,output_file_descriptor,mapped_input_file,mapped_output_file,input_file_size,output_file_size);

    std::cout << "\nSucess!" << std::endl;

    return 0;
}


