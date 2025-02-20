/**
 signalsmith-stretch~
 
 This file is part of a Max/MSP external based on Signalsmith-Stretch
 (https://github.com/Signalsmith-Audio/signalsmith-stretch).
 
 Original work [Geraint Luff / Signalsmith Audio Ltd], licensed under the MIT License.
 Modifications and Max/MSP integration (c) [Alex Bouvier], [2025].
 
 
 ----------------------------------------
 MIT License

 Copyright (c) 2025 Alex Bouvier.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */


#include <cstddef>
#include <thread>

#if __APPLE__
#include <dispatch/dispatch.h>
#elif defined(__WIN32)
#include <windows.h>
#endif


#include "../signalsmith-stretch/signalsmith-stretch.h"

#include "ext.h"
#include "ext_obex.h"
#include "ext_critical.h"
#include "ext_common.h" // contains CLAMP macro
#include "z_dsp.h"
#include "ext_buffer.h"
#include <future>
#include <shared_mutex>
#include <deque>

#include "deinterleave.hpp"
#include "common.h"

using namespace signalsmith::stretch;

typedef struct _signalsmith {
    t_pxobject l_obj;
    void* info_outlet;

    long l_chan = 0;
    int sr = 0;
        
    t_buffer_ref *l_buffer_ref = nullptr;
    std::atomic_long buffer_nc {0};

    std::unique_ptr<SignalsmithStretch<REAL>> stretch;
    long mode = 0;
    float stretch_factor = 1.0f;
    float pitch = 0.0f;
    long sample_position;
    long blocksize = 0;

#ifdef __APPLE__
    dispatch_semaphore_t chunks_semaphore;
    dispatch_semaphore_t process_semaphore;
#elif defined(_WIN32)
    HANDLE chunks_hSemaphore;
    HANDLE process_hSemaphore;
#endif
    
    std::deque<std::vector<std::vector<REAL>>> output_blocks;
    std::atomic_int num_blocks;
    int block_size;
    
    std::atomic_bool running{false};
    std::future<void> task_stretch;
    std::future<void> task_reset;

    t_critical critical_output_blocks, critical_input_buffer, critical_sema_buffer;
} t_signalsmith;


void signalsmith_perform64(t_signalsmith *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void signalsmith_dsp64(t_signalsmith *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);

void *signalsmith_new(t_symbol *s_input, long chan, long mode);
void signalsmith_free(t_signalsmith *x);

void signalsmith_update_buffer(t_signalsmith *x);
t_max_err signalsmith_notify(t_signalsmith *x, t_symbol *s, t_symbol *msg, void *sender, void *data);
void signalsmith_dblclick(t_signalsmith *x);

void signalsmith_assist(t_signalsmith *x, void *b, long m, long a, char *s);

void signalsmith_create_stretcher(t_signalsmith *x, long num_channels, long mode);
void signalsmith_delete_stretcher(t_signalsmith *x);

t_max_err signalsmith_stretch_factor_set(t_signalsmith *x, t_object *attr, long argc, t_atom *argv);
t_max_err signalsmith_pitch_set(t_signalsmith *x, t_object *attr, long argc, t_atom *argv);
t_max_err signalsmith_position_set(t_signalsmith *x, t_object *attr, long argc, t_atom *argv);
t_max_err signalsmith_mode_set(t_signalsmith *x, t_object *attr, long argc, t_atom *argv);

void signalsmith_get_input_latency(t_signalsmith *x);
void signalsmith_get_output_latency(t_signalsmith *x);
void signalsmith_reset(t_signalsmith *x);
void signalsmith_buffer_notify(t_signalsmith *x);
std::tuple<bool, long> signalsmith_extract_samples(t_signalsmith *x,
                                                   std::vector<std::vector<REAL>> &output,
                                                   long position,
                                                   long blocksize,
                                                   long min_blocksize = 4);
static t_class *signalsmith_class;

void ext_main(void *r)
{
    t_class *c = class_new("signalsmith-stretch~", 
                           (method)signalsmith_new, 
                           (method)signalsmith_free,
                           sizeof(t_signalsmith), 0L,
                           A_SYM,
                           A_DEFLONG,
                           A_DEFLONG,
                           0);

    CLASS_ATTR_FLOAT(c, "stretch_factor", 0, t_signalsmith, stretch_factor);
    CLASS_ATTR_ACCESSORS(c, "stretch_factor", NULL, signalsmith_stretch_factor_set);

    CLASS_ATTR_FLOAT(c, "pitch", 0, t_signalsmith, pitch);
    CLASS_ATTR_ACCESSORS(c, "pitch", NULL, signalsmith_pitch_set);
    
    CLASS_ATTR_LONG(c, "position", 0, t_signalsmith, sample_position);
    CLASS_ATTR_ACCESSORS(c, "position", NULL, signalsmith_position_set);

    CLASS_ATTR_LONG(c, "mode", 0, t_signalsmith, mode);
    CLASS_ATTR_ACCESSORS(c, "mode", NULL, signalsmith_mode_set);

    class_addmethod(c, (method)signalsmith_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)signalsmith_assist, "assist", A_CANT, 0);
    class_addmethod(c, (method)signalsmith_dblclick, "dblclick", A_CANT, 0);
    class_addmethod(c, (method)signalsmith_notify, "notify", A_CANT, 0);
    class_addmethod(c, (method)signalsmith_reset, "reset", 0);
    class_addmethod(c, (method)signalsmith_get_input_latency, "get_input_latency", 0);
    class_addmethod(c, (method)signalsmith_get_output_latency, "get_output_latency", 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    
    signalsmith_class = c;
    
    const char* version = "v1.0.2";
#if defined(__ARM_NEON__)
            post("signalsmith-stretch~ %s by Alex Bouvier - Based on [signalsmith-stretch] by [Geraint Luff / Signalsmith Audio Ltd]. ARM NEON optimised. MIT License", version);
#elif defined(__AVX2__)
            post("signalsmith-stretch~ %s by Alex Bouvier - Based on [signalsmith-stretch] by [Geraint Luff / Signalsmith Audio Ltd]. AVX2 optimised. MIT License", version);
#elif defined(__AVX__)
            post("signalsmith-stretch~ %s  by Alex Bouvier - Based on [signalsmith-stretch] by [Geraint Luff / Signalsmith Audio Ltd]. AVX optimised. MIT License", version);
#else
            post("signalsmith-stretch~ %s  by Alex Bouvier - Based on [signalsmith-stretch] by [Geraint Luff / Signalsmith Audio Ltd]. MIT License", version);
#endif
}

void *signalsmith_new(t_symbol *s_input_buffer, 
                      long chan,
                      long mode)
{
    t_signalsmith *x = (t_signalsmith*)object_alloc(signalsmith_class);
    dsp_setup((t_pxobject *)x, 1);

#ifdef __APPLE__
            x->chunks_semaphore = dispatch_semaphore_create(0);
            x->process_semaphore = dispatch_semaphore_create(0);
#elif defined(_WIN32)
            x->chunks_hSemaphore = CreateSemaphore(nullptr, 0, 1, nullptr);
            x->process_hSemaphore = CreateSemaphore(nullptr, 0, 1, nullptr);
#endif
    

    x->sr = (int)sys_getsr();
    x->block_size = sys_getblksize();
    
    x->mode = (int)mode;
    x->stretch_factor = 1.0f;
    x->pitch = 0.0f;
    x->sample_position = 0;
    
    x->l_chan = chan > 0 ? MIN(MAX(chan, 1), MAX_BUFFER_CHANNEL) : 1;  // num channels: [1,MAX_BUFFER_CHANNEL]
    
    x->info_outlet = outlet_new((t_object *)x, NULL);

    for(int c = 0; c < x->l_chan; ++c)
        outlet_new((t_object *)x, "signal");
    outlet_new((t_object *)x, "signal");    // for position
    outlet_new((t_object *)x, "signal");    // for blocksize
    

    critical_new(&x->critical_output_blocks);
    critical_new(&x->critical_input_buffer);
    critical_new(&x->critical_sema_buffer);
    
    if (!x->l_buffer_ref)
        x->l_buffer_ref = buffer_ref_new((t_object *)x, s_input_buffer);
    else
        buffer_ref_set(x->l_buffer_ref, s_input_buffer);
    
    signalsmith_update_buffer(x);

    
    return (x);
}

void signalsmith_free(t_signalsmith *x)
{
    dsp_free((t_pxobject *)x);
    signalsmith_delete_stretcher(x);
    

#ifdef __APPLE__
    dispatch_release(x->chunks_semaphore);
    dispatch_release(x->process_semaphore);
#elif defined(_WIN32)
    CloseHandle(x->chunks_hSemaphore);
    CloseHandle(x->process_hSemaphore);
#endif
    

    object_free(x->l_buffer_ref);
    
    critical_free(x->critical_output_blocks);
    critical_free(x->critical_input_buffer);
    critical_free(x->critical_sema_buffer);
}

// ------

t_max_err signalsmith_stretch_factor_set(t_signalsmith *x, t_object *attr, long argc, t_atom *argv){
    float factor = atom_getfloat(argv);
    x->stretch_factor = MAX(factor, 0.0f);
    return 0;
}

t_max_err signalsmith_pitch_set(t_signalsmith *x, t_object *attr, long argc, t_atom *argv){
    float val = atom_getfloat(argv);
    x->pitch = val;
    signalsmith_reset(x);
    return 0;
}

t_max_err signalsmith_position_set(t_signalsmith *x, t_object *attr, long argc, t_atom *argv){
    long val = atom_getlong(argv);
    x->sample_position = val;
    return 0;
}


t_max_err signalsmith_mode_set(t_signalsmith *x, t_object *attr, long argc, t_atom *argv){
    critical_enter(x->critical_input_buffer);
    critical_enter(x->critical_sema_buffer);

    long val = atom_getlong(argv);

    x->mode = (int)val;
    signalsmith_create_stretcher(x, (int)MIN(x->l_chan, x->buffer_nc.load()), x->mode);

    critical_exit(x->critical_sema_buffer);
    critical_exit(x->critical_input_buffer);
    return 0;
}

// ------


void signalsmith_get_input_latency(t_signalsmith *x){
    critical_enter(x->critical_input_buffer);
    t_atom_long latency = x->stretch ? (long)x->stretch->inputLatency() : 0;
    t_atom av[2];
    atom_setsym(&av[0], gensym("input_latency"));
    atom_setlong(&av[1], latency);
    outlet_list(x->info_outlet, gensym("list"), 2, av);
    critical_exit(x->critical_input_buffer);
}

void signalsmith_get_output_latency(t_signalsmith *x){
    critical_enter(x->critical_input_buffer);
    t_atom_long latency = x->stretch ? (long)x->stretch->outputLatency() : 0;
    t_atom av[2];
    atom_setsym(&av[0], gensym("output_latency"));
    atom_setlong(&av[1], latency);
    outlet_list(x->info_outlet, gensym("list"), 2, av);
    critical_exit(x->critical_input_buffer);
}

void signalsmith_dsp64(t_signalsmith *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    x->sr = (int)samplerate;
    dsp_add64(dsp64, (t_object *)x, (t_perfroutine64)signalsmith_perform64, 0, NULL);
}

void signalsmith_dblclick(t_signalsmith *x)
{
    buffer_view(buffer_ref_getobject(x->l_buffer_ref));
}

void signalsmith_assist(t_signalsmith *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_OUTLET){
        if(a < x->l_chan){
            snprintf(s, 26 + std::to_string(a).length(), "(signal) output channel %ld", a);
        }
        else if( a == x->l_chan + 0){
            snprintf(s, 25, "(signal) sample position");
        }
        else if( a == x->l_chan + 1){
            snprintf(s, 19, "(signal) blocksize");
        }
        else if( a == x->l_chan + 2){
            snprintf(s, 6, "infos");
        }
    }
    else {
        switch (a) {
            case 0: snprintf(s, 20, "(signal) start/stop");    break;
        }
    }
}

// -----------------


void signalsmith_reset(t_signalsmith*x){
    if(x->task_reset.valid())
        x->task_reset.wait();

    x->task_reset = std::async(std::launch::async, std::bind([&](t_signalsmith* x){
        if(x->stretch){
            x->stretch->reset();
        }

        critical_enter(x->critical_output_blocks);
        x->num_blocks = 0;
        x->output_blocks.clear();
        critical_exit(x->critical_output_blocks);

        bool trial = true;
        do{
            trial = critical_tryenter(x->critical_sema_buffer);
            if(!trial){
#ifdef __APPLE__
        dispatch_release(x->chunks_semaphore);
        x->chunks_semaphore = dispatch_semaphore_create(0);
#elif defined(_WIN32)
        CloseHandle(x->chunks_hSemaphore);
        x->chunks_hSemaphore = CreateSemaphore(nullptr, 0, 1, nullptr);
#endif
        critical_exit(x->critical_sema_buffer);
            }
        }while(trial);
        
        // launch process task
#ifdef __APPLE__
        dispatch_semaphore_signal(x->process_semaphore);
#elif defined(_WIN32)
        ReleaseSemaphore(x->process_hSemaphore, 1, nullptr);
#endif
    }, x));
                               
}

void signalsmith_update_buffer(t_signalsmith *x)
{
    // update buffer nc
    t_buffer_obj *buffer = buffer_ref_getobject(x->l_buffer_ref);
    if (buffer) {
        x->buffer_nc = buffer_getchannelcount(buffer);
        
        if(x->buffer_nc > MAX_BUFFER_CHANNEL){
            error("signalsmith-stretch~ error: cannot read more than %i channels, got %i channels.", MAX_BUFFER_CHANNEL, x->buffer_nc.load());
            
            x->buffer_nc = 0;
        }
    }
    else{
        x->buffer_nc = 0;
    }
    
    signalsmith_create_stretcher(x, (int)MIN(x->l_chan, x->buffer_nc.load()), x->mode);
    
    signalsmith_reset(x);
}

t_max_err signalsmith_notify(t_signalsmith *x, t_symbol *s, t_symbol *msg, void *sender, void *data)
{
    signalsmith_update_buffer(x);
    return buffer_ref_notify(x->l_buffer_ref, s, msg, sender, data);
}


// -----------------

void signalsmith_delete_stretcher(t_signalsmith *x){
    if(x->stretch){
        x->running = false;
        if(x->task_stretch.valid()){
            x->task_stretch.wait();
        }
    }
    x->stretch = nullptr;
}



void signalsmith_create_stretcher(t_signalsmith *x, long num_channels, long mode){
    signalsmith_delete_stretcher(x);
    
    if(num_channels > 0){
        x->stretch.reset(new SignalsmithStretch<REAL>());
        if(x->stretch){
            if(mode==2){
                x->stretch->configure((int)num_channels, (float)x->sr*0.12f, (float)x->sr*0.015);
            }
            else if (mode == 1){
                x->stretch->presetCheaper((int)num_channels, (float)x->sr);
            }
            else if (mode == 3){
                x->stretch->configure((int)num_channels, (float)x->sr*0.1f*2.0f, (float)x->sr*0.04f*2.0f);
            }
            else{
                x->stretch->presetDefault((int)num_channels, (float)x->sr);
            }
            
            x->task_stretch = std::async(std::launch::async, std::bind([](t_signalsmith* x){
                const long MIN_BLOCKSIZE = 4;
                
                x->running = true;

                std::vector<std::vector<REAL>> extracted_buffer;
                
                critical_enter(x->critical_input_buffer);
                std::vector<std::vector<REAL>> output_ptr(x->buffer_nc);
                for (int i = 0; i < x->buffer_nc; i++) {
                    output_ptr[i] = std::vector<REAL>(OUTPUT_STRETCH_BUFFER_SIZE);
                }
                critical_exit(x->critical_input_buffer);

                //launch 1st computation
#ifdef __APPLE__
                dispatch_semaphore_signal(x->process_semaphore);
#elif defined(_WIN32)
                ReleaseSemaphore(x->process_hSemaphore, 1, nullptr);
#endif
                int prev_chunk_size = x->block_size;
                std::vector<std::vector<REAL>> data(x->buffer_nc);
                for(int c = 0; c < x->buffer_nc; ++c){
                    data[c].resize(prev_chunk_size);
                }
                
                while(x->running){
                   
                    bool process = false;
            #ifdef __APPLE__
                    process = dispatch_semaphore_wait(x->process_semaphore, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_MSEC)) == 0;
            #elif defined(_WIN32)
                    process = WaitForSingleObject(x->process_hSemaphore, 10) == WAIT_OBJECT_0;
            #endif
                    
                    if(process){
                        critical_enter(x->critical_input_buffer);

                        x->stretch->setTransposeSemitones(x->pitch);
                        double  stretch_factor = x->stretch_factor;
                        int input_latency = x->stretch->inputLatency();
                        long block_samples = MAX((long)(stretch_factor * OUTPUT_STRETCH_BUFFER_SIZE), MIN_BLOCKSIZE);
                        auto [can_compute, pos] = signalsmith_extract_samples(x, extracted_buffer, x->sample_position, block_samples, MIN_BLOCKSIZE);
                        
                        
                        /*
                         can_compute if extraction done and buffer almost empty
                         update position
                         update block size
                         */
                        x->sample_position = pos;
                        x->blocksize = block_samples + input_latency;
                        

                        if(can_compute)
                        {
                            x->stretch->process(extracted_buffer, (int)block_samples, output_ptr, OUTPUT_STRETCH_BUFFER_SIZE);

                            auto chunk_size = sys_getblksize();
                            assert((OUTPUT_STRETCH_BUFFER_SIZE%chunk_size)==0);

                            critical_enter(x->critical_output_blocks);
                            
                            // resize data if needed
                            if(prev_chunk_size != chunk_size){
                                prev_chunk_size = chunk_size;
                                std::vector<std::vector<REAL>> data(x->buffer_nc);
                                for(int c = 0; c < x->buffer_nc; ++c){
                                    data[c].resize(prev_chunk_size);
                                }
                            }
                            
                            for(int i = 0; i < OUTPUT_STRETCH_BUFFER_SIZE/chunk_size; ++i){
                                for(int c = 0; c < x->buffer_nc; ++c){
                                    std::copy(output_ptr[c].begin() + (i * chunk_size), output_ptr[c].begin() + (i * chunk_size + chunk_size), data[c].begin());
                                }
                                
                                x->output_blocks.push_back(data);
                                x->num_blocks++;
#ifdef __APPLE__
                                dispatch_semaphore_signal(x->chunks_semaphore);
#elif defined(_WIN32)
                                ReleaseSemaphore(x->chunks_hSemaphore, 1, nullptr);
#endif
                            }
                            critical_exit(x->critical_output_blocks);

                            x->sample_position += block_samples;
                        }
                        else{
                            // if cannot extract any more samples, output silence
                            critical_enter(x->critical_output_blocks);
                            
                            //  silence
                            std::vector<std::vector<REAL>> data(x->buffer_nc);
                            for(int c = 0; c < x->buffer_nc; ++c){
                                data[c].resize(x->block_size);
                                std::fill(data[c].begin(), data[c].end(), 0.0f);
                            }
                            
                            for(int i = 0; i < OUTPUT_STRETCH_BUFFER_SIZE/x->blocksize; ++i){
                                x->output_blocks.push_back(data);
                                x->num_blocks++;
#ifdef __APPLE__
                                dispatch_semaphore_signal(x->chunks_semaphore);
#elif defined(_WIN32)
                                ReleaseSemaphore(x->chunks_hSemaphore, 1, nullptr);
#endif                                
                            }
                            critical_exit(x->critical_output_blocks);
                        }
                        critical_exit(x->critical_input_buffer);
                    }
                }
            }, x));
        }
    }
    else{
        x->stretch = nullptr;
    }
}

void signalsmith_perform64(t_signalsmith *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    t_double    *in = ins[0];
    bool is_on = in[0] != 0. ? true : false;

    // reset if blocksize has changed
    if(sampleframes != x->block_size){
        x->block_size = (int)sampleframes;
        signalsmith_reset(x);
    }

    if(is_on && x->buffer_nc > 0){
        // wait for chunks
        bool woken = false;
        while(!woken){
            critical_enter(x->critical_sema_buffer);
#ifdef __APPLE__
            woken = dispatch_semaphore_wait(x->chunks_semaphore, dispatch_time(DISPATCH_TIME_NOW, 1 * NSEC_PER_SEC)) == 0;
#elif defined(_WIN32)
            woken = WaitForSingleObject(x->chunks_hSemaphore, 1) == WAIT_OBJECT_0;
#endif
        critical_exit(x->critical_sema_buffer);
        }
        
        if(x->num_blocks > 0){
            // if reset here, check empty after critical section
            critical_enter(x->critical_output_blocks);
            auto data = x->output_blocks.empty() ? std::vector<std::vector<REAL>>(): x->output_blocks.front();
            x->output_blocks.pop_front();
            critical_exit(x->critical_output_blocks);
            
            for(long c = 0; c < data.size(); ++c){
                for(size_t i = 0; i < sampleframes; ++i)
                    outs[c][i] = data[c][i];
            }
            
            // silence the remaining channels
            for(long i = data.size(); i < x->l_chan; ++i)
                std::fill(&(outs[i][0]), &(outs[i][0]) + sampleframes, 0.0);
            
            x->num_blocks--;
        }
        else{
            //silence all channels
            for(long i = 0; i < x->l_chan; ++i)
                std::fill(&(outs[i][0]), &(outs[i][0]) + sampleframes, 0.0);
        }
    }
    else{
        //silence all channels
        for(long i = 0; i < x->l_chan; ++i)
            std::fill(&(outs[i][0]), &(outs[i][0]) + sampleframes, 0.0);
    }

    
    // position + blocksize. always output these parameters
    long pos = x->sample_position;
    long bs = x->blocksize;
    for(size_t i = 0; i < sampleframes; ++i){
        outs[x->l_chan][i] = pos;
        outs[x->l_chan + 1][i] = bs;
    }
    
    // Signal process when half chunks remains
    if(x->num_blocks == (OUTPUT_STRETCH_BUFFER_SIZE/sampleframes)/2){
#ifdef __APPLE__
                                dispatch_semaphore_signal(x->process_semaphore);
#elif defined(_WIN32)
                                ReleaseSemaphore(x->process_hSemaphore, 1, nullptr);
#endif
    }

}



/**
 signalsmith stretch needs inputLatency more sample
 
 - Parameters:
 - x: current instance
 - output: output vectors
 - position: desired start in buffer (in samples)
 - blocksize: desired blocksize to extract
 - min_blocksize: min size of block to extract
 */
std::tuple<bool, long> signalsmith_extract_samples(t_signalsmith *x,
                                                   std::vector<std::vector<REAL>> &output,
                                                   long position,
                                                   long blocksize,
                                                   long min_blocksize)
{
    if(!x->l_buffer_ref)
        return {false, position};

    t_buffer_obj *buffer = buffer_ref_getobject(x->l_buffer_ref);
    if (buffer) {
        auto buffer_sr = buffer_getsamplerate(buffer);
        float sr_ratio = (float) buffer_sr / (float)x->sr;
        long fc = buffer_getframecount(buffer) * sr_ratio;

        if(fc == 0 || fc < blocksize || blocksize < min_blocksize){
            return {false, position};
        }
        int input_latency = x->stretch->inputLatency();
        

        
        long start = position - input_latency;
        if(start < 0){
            start = 0;
            position = input_latency;
        }
        
        if(start >= fc){
            return {false, position};
        }
        
        long end = start + blocksize + input_latency;
        long add_samples = 0;
        if (end >= fc) {
            end = fc-1;
            add_samples = blocksize + input_latency - (end - start);
        }
        
        assert(start>= 0 && start < fc);
        assert(end>= input_latency && end < fc);
        assert(end >= start);
        assert((end + add_samples - start) == blocksize + input_latency);
        float* tab = buffer_locksamples(buffer);
        if(tab){
            deinterleave(tab + start*x->buffer_nc, output, end-start, x->buffer_nc);
        }
        buffer_unlocksamples(buffer);

        if(add_samples > 0){
            std::vector<REAL> silence(add_samples);
            for(auto& channel : output){
                channel.insert(channel.end(), silence.begin(), silence.end());
            }
        }
            
        return {true, position};
    }
    else
        return {false, position};
}
