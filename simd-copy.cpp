// simd-copy.cpp
// 
// cl.exe /EHsc /Ox simd-copy.cpp
// g++ -std=c++11 -O3 simd-copy.cpp
//
// or
// 
// cl.exe /EHsc /Ox /arch:AVX2 simd-copy.cpp
// g++ -std=c++11 -O3 -march=core-avx2 -mtune=core-avx2 -mavx2 

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <immintrin.h>
#include <cassert>
#include "cgutil/program_options.h"
#include "cgutil/timer.h"

#ifndef SUPPORT_AVX
#  define SUPPORT_AVX 1
#endif

template<typename T>
T* align(T* p, std::size_t aligned_to)
{
	std::size_t address = reinterpret_cast<std::size_t>(p);
	while(address % 256 != aligned_to)
		++address;
	return reinterpret_cast<T*>(address);
}

// ----------------------------------------------------------------------------
//
std::size_t kDefaultNumFloats = 16 * 1024;
std::size_t kDefaultTotalFloats = 65636 * kDefaultNumFloats;
std::size_t gNumFloats = kDefaultNumFloats;
std::size_t gTotalFloats = kDefaultTotalFloats;
float gCheckValue = 1.f;
bool gHasAvx = true;
bool gHtmlOut = true;

void MemCopy(float* d, float const* s)
{
	std::memcpy(d, s, gNumFloats * sizeof(float));
}

void StdCopy(float* d, float const* s)
{
	std::copy(s, s + gNumFloats, d);
}

void SimpleCopy(float* d, float const* s)
{
	for(int i = 0; i < gNumFloats; ++i)
	{
		*d++ = *s++;
	}
}

void UnalignedSseCopy(float* d, float const* s)
{
	for(int i = 0; i < gNumFloats; i += 4)
	{
		__m128 v = _mm_loadu_ps(&s[i]);
		_mm_storeu_ps(&d[i], v);
	}
}

void AlignedSseCopy(float* d, float const* s)
{
	for(int i = 0; i < gNumFloats; i += 4)
	{
		__m128 v = _mm_load_ps(&s[i]);
		_mm_store_ps(&d[i], v);
	}
}

void AlignedSseNonTemporalCopy(float* d, float const* s)
{
	for(int i = 0; i < gNumFloats; i += 4)
	{
		__m128 v = _mm_load_ps(&s[i]);
		_mm_stream_ps(&d[i], v);
	}
}

#if SUPPORT_AVX
void UnalignedAvxCopy(float* d, float const* s)
{
	for(int i = 0; i < gNumFloats; i += 8)
	{
		__m256 v = _mm256_loadu_ps(&s[i]);
		_mm256_storeu_ps(&d[i], v);
	}
}

void AlignedAvxCopy(float* d, float const* s)
{
	for(int i = 0; i < gNumFloats; i += 8)
	{
		__m256 v = _mm256_load_ps(&s[i]);
		_mm256_store_ps(&d[i], v);
	}
}

void AlignedAvxNonTemporalCopy(float* d, float const* s)
{
	for(int i = 0; i < gNumFloats; i += 8)
	{
		__m256 v = _mm256_load_ps(&s[i]);
		_mm256_stream_ps(&d[i], v);
	}
}
#endif

void NullCopy(float* d, float const* s)
{}

// ----------------------------------------------------------------------------
//
template<void(*f)(float*, float const*)>
void Run(char const* name, std::size_t alignment, float* d, float const* s)
{
	d = align(d, alignment);
	s = align(s, alignment);
	std::fill(d, d + gNumFloats, 0.f);

	cgutil::timer t;
	for(std::size_t i = 0; i < gTotalFloats; i += gNumFloats)
	{
		f(d, s);
	}
	float time = t.elapsed();

	for(std::size_t i = 0; i < gNumFloats; ++i)
	{
		if(d[i] != s[i])
		{
			std::cerr << "Error in " << name << " " << d[i] << " != " << s[i] << std::endl;
			std::exit(1);
		}
	}

	std::cerr << name 
			  << " (" << alignment << ") took " 
			  << time << " seconds." 
			  << std::endl
	;

	std::cout << "," << time;
}

template<>
void Run<NullCopy>(char const* name, std::size_t alignment, float* d, float const* s)
{
	std::cout << "," << 0;
}


// ----------------------------------------------------------------------------
//
void print_usage()
{
	std::cerr << "Usage:\n" 
			  << "copy [options]\n"
			  << "num-floats=<number of float in memory>    default (" << kDefaultNumFloats << ")\n"
			  << "total-floats=<number of floats total>     default (" << kDefaultTotalFloats << ")\n"
			  << "check-value=<any value to check against>  default (" << gCheckValue << ")\n"
			  << "enable-avx=<true/false>                   default (" << std::boolalpha << gHasAvx << ")\n"
			  << "report-html=<true/false>                  default (" << std::boolalpha << gHtmlOut << ")\n"
			  << std::endl;

}

// ----------------------------------------------------------------------------
//
int main(int argc, char** argv)
{
	cgutil::program_options opts;
	opts.add("num-floats", gNumFloats);
	opts.add("num-floats", gNumFloats);
	opts.add("total-floats", gTotalFloats);
	opts.add("check-value", gCheckValue);
	opts.add("enable-avx", gHasAvx);
	
	try
	{
		opts.parse(argc, argv);
	}
	catch(std::runtime_error e)
	{
		std::cerr << e.what() << std::endl;
		print_usage();
		std::exit(1);
	}

	if(gTotalFloats < gNumFloats)
	{
		std::cerr << "total-floats must be greater than num-floats" << std::endl;
		print_usage();
		return 0;
	}

	if(gHtmlOut)
	{
		std::cout <<
		   "<html>\n"
		   "  <head>\n"
		   "    <script type=\"text/javascript\" src=\"https://www.google.com/jsapi\"></script>\n"
		   "    <script type=\"text/javascript\">\n"
		   "      google.load(\"visualization\", \"1\", {packages:[\"corechart\"]});\n"
		   "      google.setOnLoadCallback(drawChart);\n"
		   "      function drawChart() {\n"
		   "        var data = google.visualization.arrayToDataTable([\n"
		;
	}

	// Allocate 64 megs worth of floats;
	std::vector<float> source(gNumFloats + 0x100, gCheckValue);
	std::vector<float> dest(gNumFloats + 0x100, 0.f);

	std::cout << "[\'Alignment\',\'std::memcpy\',\'std::copy\',\'for-loop\',\'Unaligned Sse\',\'Unaligned Avx\',\'Aligned Sse\',\'Aligned Sse Stream\',\'Aligned Avx\',\'Aligned Avx Stream\'";
	for(std::size_t alignment = 4; alignment <= 64; ++alignment)
	{
		std::cout << "],\n" << "[" << alignment;

		Run<MemCopy>("std::memcpy", alignment, dest.data(), source.data());
		Run<StdCopy>("std::copy", alignment, dest.data(), source.data());
		Run<SimpleCopy>("for-loop", alignment, dest.data(), source.data());
		Run<UnalignedSseCopy>("Unaligned Sse", alignment, dest.data(), source.data());

	#if SUPPORT_AVX
		if(gHasAvx)
		{
			Run<UnalignedAvxCopy>("Unaligned Avx", alignment, dest.data(), source.data());
		}
		else
	#endif
		{
			Run<NullCopy>("Unaligned Avx", alignment, source.data(), source.data());
		}

		if(alignment % 16 == 0)
		{
			Run<AlignedSseCopy>("Aligned Sse", alignment, dest.data(), source.data());
			Run<AlignedSseNonTemporalCopy>("Aligned Sse Stream", alignment, dest.data(), source.data());
		}
		else
		{
			Run<NullCopy>("Aligned Sse", alignment, source.data(), source.data());
			Run<NullCopy>("Aligned Sse Stream", alignment, source.data(), source.data());
		}
	#if SUPPORT_AVX
		if(alignment % 32 == 0 && gHasAvx)
		{
			Run<AlignedAvxCopy>("Aligned Avx", alignment, dest.data(), source.data());
			Run<AlignedAvxNonTemporalCopy>("Aligned Avx Stream", alignment, dest.data(), source.data());
		}
		else
	#endif
		{
			Run<NullCopy>("Aligned Avx", alignment, source.data(), source.data());
			Run<NullCopy>("Aligned Avx Stream", alignment, source.data(), source.data());
		}       
	}

	std::cout << "]" << std::endl;

	if(gHtmlOut)
	{
		std::cout <<
			"        ]);\n"
			"        var options = {\n"
			"          title: 'Alignment vs. Run Time'\n"
			"        };\n"
			"        var chart = new google.visualization.LineChart(document.getElementById('chart_div'));\n"
			"        chart.draw(data, options);\n"
			"      }\n"
			"    </script>\n"
			"  </head>\n"
			"  <body>\n"
			"    <div id=\"chart_div\" style=\"width: 900px; height: 500px;\"></div>\n"
			"  </body>\n"
			"</html>\n"
		;
	}


	return 0;
}