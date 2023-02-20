
#include <chrono>

#include "app.h"
#include "app_time.h"
#include "app_camera.h"
#include "widgets.h"

#include "vec.h"
#include "mat.h"

#include "program.h"
#include "uniforms.h"
#include "texture.h"
#include "draw.h"

#include "mesh.h"
#include "wavefront.h"
#include "wavefront_fast.h"

#include "orbiter.h"

struct stats
{
    float draw_time;
    float bench1_time;
    float bench2_time;
    float bench3_time;
    float bench4_time;
};

const int MAX_FRAMES= 3;

struct Bench : public AppCamera
{
    Bench( ) : AppCamera(1024, 512, 4, 3)
    {
        vsync_off();
    }
    
    int init( )
    {
        //~ m_mesh= read_mesh_fast("data/robot.obj");
        //~ m_mesh= read_indexed_mesh_fast("/home/jciehl/scenes/bistro/exterior.obj");
        //~ m_mesh= read_indexed_mesh_fast("/home/jciehl/scenes/bistro/exterior.obj");
        //~ m_mesh= read_mesh_fast("/home/jciehl/scenes/quixel/quixel.obj");
        //~ m_mesh= read_indexed_mesh_fast("/home/jciehl/scenes/quixel/quixel.obj");
        m_mesh= read_indexed_mesh_fast("/home/jciehl/scenes/sponza-intel/export.obj");
        //~ m_mesh= read_mesh_fast("/home/jciehl/scenes/sponza-intel/export.obj");
        //~ m_mesh= read_indexed_mesh_fast("/home/jciehl/scenes/rungholt/rungholt.obj");
        //~ m_mesh= read_indexed_mesh_fast("/home/jciehl/scenes/san-miguel/san-miguel.obj");
        
        Point pmin, pmax;
        m_mesh.bounds(pmin, pmax);
        camera().lookat(pmin, pmax);
        
        m_grid_texture= read_texture(0, "data/grid.png");
        
        m_program_texture= read_program("tutos/bench/vertex2.glsl");
        program_print_errors(m_program_texture);
        
        m_program_cull= read_program("tutos/bench/vertex_cull.glsl");
        program_print_errors(m_program_cull);
        
        m_program_rasterizer= read_program("tutos/bench/rasterizer.glsl");
        program_print_errors(m_program_rasterizer);
        
        if(program_errors(m_program_texture) || program_errors(m_program_cull) || program_errors(m_program_rasterizer))
            return -1;
        
        // bench 1 : triangles mal orientes / cull rate
    #if 1
        // triangles non indexes...
        m_triangles.create(GL_TRIANGLES);
        for(int i= 0; i < 1024*1024; i++)
        {
            // back face culled
            m_triangles.texcoord(0, 0);
            m_triangles.normal(0, 0, 1);
            m_triangles.vertex(-0.1, -0.1, 0);
            
            m_triangles.texcoord(1, 1);
            m_triangles.normal(0, 0, 1);
            m_triangles.vertex( 0.1,  0.1, 0);
            
            m_triangles.texcoord(1, 0);
            m_triangles.normal(0, 0, 1);
            m_triangles.vertex( 0.1, -0.1, 0);
        }
    #else
    
        // triangles indexes
        m_triangles.create(GL_TRIANGLES);
        // back face culled
        m_triangles.texcoord(0, 0);
        m_triangles.normal(0, 0, 1);
        unsigned a= m_triangles.vertex(-0.1, -0.1, 0);
        
        m_triangles.texcoord(1, 1);
        m_triangles.normal(0, 0, 1);
        unsigned b= m_triangles.vertex( 0.1,  0.1, 0);
        
        m_triangles.texcoord(1, 0);
        m_triangles.normal(0, 0, 1);
        unsigned c= m_triangles.vertex( 0.1, -0.1, 0);
        
        for(int i= 0; i < 1024*1024; i++)
            m_triangles.triangle(a, b, c);
    #endif
    
        m_vao_triangles= m_triangles.create_buffers(/* use_texcoord */ true, /* use_normal */ true, /* use_color */ false, /* use_material_index */ false);
        
        //
        m_frame= 0;
        glGenQueries(MAX_FRAMES, m_time_query);
        glGenQueries(MAX_FRAMES, m_vertex_query);
        glGenQueries(MAX_FRAMES, m_fragment_query);
        glGenQueries(MAX_FRAMES, m_bench1_query);
        glGenQueries(MAX_FRAMES, m_bench2_query);
        glGenQueries(MAX_FRAMES, m_bench3_query);
        glGenQueries(MAX_FRAMES, m_bench4_query);
        
        // initialise les requetes... simplifie la collecte sur la 1ere frame...
        for(int i= 0; i < MAX_FRAMES; i++)
        {
            glBeginQuery(GL_TIME_ELAPSED, m_time_query[i]); glEndQuery(GL_TIME_ELAPSED);
            glBeginQuery(GL_VERTEX_SHADER_INVOCATIONS_ARB, m_vertex_query[i]); glEndQuery(GL_VERTEX_SHADER_INVOCATIONS_ARB);
            //~ glBeginQuery(GL_FRAGMENT_SHADER_INVOCATIONS_ARB, m_fragment_query[i]); glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS_ARB);
            glBeginQuery(GL_SAMPLES_PASSED, m_fragment_query[i]); glEndQuery(GL_SAMPLES_PASSED);
            glBeginQuery(GL_TIME_ELAPSED, m_bench1_query[i]); glEndQuery(GL_TIME_ELAPSED);
            glBeginQuery(GL_TIME_ELAPSED, m_bench2_query[i]); glEndQuery(GL_TIME_ELAPSED);
            glBeginQuery(GL_TIME_ELAPSED, m_bench3_query[i]); glEndQuery(GL_TIME_ELAPSED);
            glBeginQuery(GL_TIME_ELAPSED, m_bench4_query[i]); glEndQuery(GL_TIME_ELAPSED);
        }
        
        // etat openGL par defaut
        glClearColor(0.2f, 0.2f, 0.2f, 1.f);
        
        glClearDepth(1.f);
        //~ glDepthFunc(GL_LEQUAL);
        glDepthFunc(GL_LESS);
        glEnable(GL_DEPTH_TEST);
        //~ glDisable(GL_DEPTH_TEST);
        
        glFrontFace(GL_CCW);
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        
        glDisable(GL_BLEND);
        
        return 0;
    }
    
    int quit( )
    {
        FILE *out= fopen("bench3.txt", "wt");
        if(out)
        {
            for(auto& stats : m_stats)
            {
                fprintf(out, "%f %f %f %f %f\n", 
                    stats.draw_time,     // 1 time
                    stats.bench1_time,  // 2 discard
                    stats.bench2_time,  // 3 rasterizer
                    stats.bench3_time,  // 4 cull
                    stats.bench4_time); // 5 fragments
            }
            
            fclose(out);
        }
        
        m_mesh.release();
        release_program(m_program_texture);
        release_program(m_program_rasterizer);

        glDeleteTextures(1, &m_grid_texture);
        
        glDeleteQueries(MAX_FRAMES, m_time_query);
        glDeleteQueries(MAX_FRAMES, m_vertex_query);
        glDeleteQueries(MAX_FRAMES, m_fragment_query);
        glDeleteQueries(MAX_FRAMES, m_bench1_query);
        glDeleteQueries(MAX_FRAMES, m_bench2_query);
        glDeleteQueries(MAX_FRAMES, m_bench3_query);
        glDeleteQueries(MAX_FRAMES, m_bench4_query);
        
        return 0;
    }
    
    int render( )
    {
        // collecte les requetes de la frame precedente...
        {
            GLuint ready= GL_FALSE;
            glGetQueryObjectuiv(m_time_query[m_frame], GL_QUERY_RESULT_AVAILABLE, &ready);
            if(ready != GL_TRUE)
                printf("[oops] wait query, frame %d...\n", m_frame);
        }
        
        auto wait_start= std::chrono::high_resolution_clock::now();
        GLint64 gpu_draw= 0;
        glGetQueryObjecti64v(m_time_query[m_frame], GL_QUERY_RESULT, &gpu_draw);
        auto wait_stop= std::chrono::high_resolution_clock::now();
        float wait= float(std::chrono::duration_cast<std::chrono::microseconds>(wait_stop - wait_start).count()) / 1000;
        if(wait > float(0.1))
            printf("[oops] wait query %.2fms\n", wait);
        
        GLint64 gpu_vertex= 0;
        glGetQueryObjecti64v(m_vertex_query[m_frame], GL_QUERY_RESULT, &gpu_vertex);
        GLint64 gpu_fragment= 0;
        glGetQueryObjecti64v(m_fragment_query[m_frame], GL_QUERY_RESULT, &gpu_fragment);
        
        GLint64 gpu_bench1_draw= 0;
        glGetQueryObjecti64v(m_bench1_query[m_frame], GL_QUERY_RESULT, &gpu_bench1_draw);
        GLint64 gpu_bench2_draw= 0;
        glGetQueryObjecti64v(m_bench2_query[m_frame], GL_QUERY_RESULT, &gpu_bench2_draw);
        GLint64 gpu_bench3_draw= 0;
        glGetQueryObjecti64v(m_bench3_query[m_frame], GL_QUERY_RESULT, &gpu_bench3_draw);
        GLint64 gpu_bench4_draw= 0;
        glGetQueryObjecti64v(m_bench4_query[m_frame], GL_QUERY_RESULT, &gpu_bench4_draw);
        
        printf("  %.2fus draw time = %.2fus vertex (%.2fus culled) + %2.fus rasterizer\n", 
            float(gpu_draw) / 1000,
            float(gpu_bench1_draw) / 1000,
            float(gpu_bench3_draw) / 1000,
            float(gpu_bench2_draw) / 1000);
        
        float triangle_rate= float(m_mesh.triangle_count()) / float(gpu_draw) * 1000;
        
        float vertex_size= float(gpu_vertex * 32) / float(1024 * 1024);
        float vertex_rate_discard= float(gpu_vertex) / float(gpu_bench1_draw) * 1000;
        float vertex_rate_cull= float(gpu_vertex) / float(gpu_bench3_draw) * 1000;
        float vertex_bw= vertex_size / float(gpu_bench1_draw) * 1000000000;
        printf("triangle rate %.2fMt/s\n", triangle_rate);
        printf("vertex rate discard %.2fMv/s cull %.2fMv/s\n", vertex_rate_discard, vertex_rate_cull);
        printf("vertex bw %.2fMB/s\n", vertex_bw);
        
        printf("vertex %.2fM, transformed %.2fM, x%.2f\n", float(m_mesh.triangle_count()) / 1000000, float(gpu_vertex) / 1000000, 
            float(gpu_vertex) / float(m_mesh.triangle_count()));
        
        printf("fragment %.2fM\n", float(gpu_fragment) / 1000000);
        
        float fragment_rate= float(gpu_fragment) / float(gpu_draw - gpu_bench2_draw) * 1000;
        printf("fragment rate %.2fMf/s, %uus draw time %u rasterizer %u\n", fragment_rate, 
            unsigned(gpu_draw / 1000) - unsigned(gpu_bench2_draw / 1000), unsigned(gpu_draw / 1000), unsigned(gpu_bench2_draw / 1000));
        
        m_stats.push_back({
            float(gpu_draw) / 1000, 
            float(gpu_bench1_draw) / 1000,
            float(gpu_bench2_draw) / 1000,
            float(gpu_bench3_draw) / 1000,
            //~ float(gpu_bench4_draw) / 1000 });
            float(gpu_fragment) / 1000 });
        
        //
        Transform model= RotationY(global_time() / 60);
        Transform view= camera().view();
        Transform projection= camera().projection(window_width(), window_height(), 45);
        Transform mv= view * model;
        Transform mvp= projection * mv;

    #if 1
        // test 1 : normal
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(m_program_texture);
        program_uniform(m_program_texture, "mvpMatrix", mvp);
        program_uniform(m_program_texture, "mvMatrix", mv);
        program_use_texture(m_program_texture, "grid", 0, m_grid_texture);
        
        //~ glBeginQuery(GL_TIME_ELAPSED, m_time_query);
        glBeginQuery(GL_VERTEX_SHADER_INVOCATIONS_ARB, m_vertex_query[m_frame]);
        glBeginQuery(GL_SAMPLES_PASSED, m_fragment_query[m_frame]);
        //~ glBeginQuery(GL_FRAGMENT_SHADER_INVOCATIONS_ARB, m_fragment_query[frame]);
            
            m_mesh.draw(m_program_texture, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false, /* material */ false );
        
        //~ glEndQuery(GL_TIME_ELAPSED);
        glEndQuery(GL_VERTEX_SHADER_INVOCATIONS_ARB);
        //~ glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS_ARB);
        glEndQuery(GL_SAMPLES_PASSED);
    #endif
    
        // bench 1 : que les triangles
    #if 1
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_RASTERIZER_DISCARD);
        
        glUseProgram(m_program_texture);
        program_uniform(m_program_texture, "mvpMatrix", mvp);
        program_uniform(m_program_texture, "mvMatrix", mv);
        program_use_texture(m_program_texture, "grid", 0, m_grid_texture);
        
        glBeginQuery(GL_TIME_ELAPSED, m_bench1_query[m_frame]);
            
            m_mesh.draw(m_program_texture, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false, /* material */ false );
        
        glEndQuery(GL_TIME_ELAPSED);
        
        glDisable(GL_RASTERIZER_DISCARD);
        // pas efficace sur les geforces, temps equivalent au draw normal...
    #endif
    
    #if 0
        // test synthetique bench 3 : que les triangles mal orientes
        // mais triangles non indexes pour generer le meme nombre d'execution de vertex shader...
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //~ glEnable(GL_RASTERIZER_DISCARD);
        
        glBindVertexArray(m_vao_triangles);
        //~ glUseProgram(m_program_texture);
        //~ program_uniform(m_program_texture, "mvpMatrix", Identity());
        //~ program_uniform(m_program_texture, "mvMatrix", Identity());
        //~ program_use_texture(m_program_texture, "grid", 0, m_grid_texture);
        glUseProgram(m_program_cull);
        program_uniform(m_program_cull, "mvpMatrix", Identity());
        program_uniform(m_program_cull, "mvMatrix", Identity());
        program_use_texture(m_program_cull, "grid", 0, m_grid_texture);
        
        {
            int instances= m_mesh.triangle_count() / m_triangles.triangle_count();
            int n= m_mesh.triangle_count() % m_triangles.triangle_count();
            if(instances == 0 && n == 0) n= 1;
            
            glBeginQuery(GL_TIME_ELAPSED, m_bench3_query[m_frame]);
            #if 1
                // triangles non indexes
                if(instances > 0)
                    glDrawArraysInstanced(GL_TRIANGLES, 0, m_triangles.triangle_count()*3, instances);
                if(n > 0)
                    glDrawArrays(GL_TRIANGLES, 0, n*3);
            #else
            
                // triangles indexes
                if(instances > 0)
                    glDrawElementsInstanced(GL_TRIANGLES, m_triangles.triangle_count()*3, GL_UNSIGNED_INT, (const void *) 0, instances);
                if(n > 0)
                    glDrawElements(GL_TRIANGLES, n*3, GL_UNSIGNED_INT, (const void *) 0);
            #endif
            
            glEndQuery(GL_TIME_ELAPSED);
        }
        glDisable(GL_RASTERIZER_DISCARD);
    #endif
    
    #if 1
        // bench 3 : que les triangles mal orientes / elimines
        // force les sommets en dehors du frustum... 
        // equivalent ? si culling et clipping sont realises par la meme unite a la meme vitesse...
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(m_program_cull);
        program_uniform(m_program_cull, "mvpMatrix", mvp);
        //~ program_uniform(m_program_cull, "mvMatrix", mv);
        //~ program_use_texture(m_program_cull, "grid", 0, m_grid_texture);
        
        glBeginQuery(GL_TIME_ELAPSED, m_bench3_query[m_frame]);
            
            //~ m_mesh.draw(m_program_cull, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false, /* material */ false );
            m_mesh.draw(m_program_cull, /* use position */ true, /* use texcoord */ false, /* use normal */ false, /* use color */ false, /* material */ false );
        
        glEndQuery(GL_TIME_ELAPSED);
    #endif
        
        // bench 2 : que le rasterizer, pas les fragments
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //~ glDepthFunc(GL_LESS);
        
        glUseProgram(m_program_rasterizer);
        program_uniform(m_program_rasterizer, "mvpMatrix", mvp);
        
        glBeginQuery(GL_TIME_ELAPSED, m_bench2_query[m_frame]);
            
            m_mesh.draw(m_program_rasterizer, /* use position */ true, /* use texcoord */ false, /* use normal */ false, /* use color */ false, /* material */ false );
        
        glEndQuery(GL_TIME_ELAPSED);
        
        // test 1 : normal
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(m_program_texture);
        program_uniform(m_program_texture, "mvpMatrix", mvp);
        program_uniform(m_program_texture, "mvMatrix", mv);
        program_use_texture(m_program_texture, "grid", 0, m_grid_texture);
        
        glBeginQuery(GL_TIME_ELAPSED, m_time_query[m_frame]);
            
            m_mesh.draw(m_program_texture, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false, /* material */ false );
        
        glEndQuery(GL_TIME_ELAPSED);
        
        // recycle les requetes...
        m_frame= (m_frame + 1) % MAX_FRAMES;
        
        return 1;   // on continue
    }
    
protected:
    std::vector<stats> m_stats;

    Mesh m_mesh;
    Mesh m_triangles;
    GLuint m_program_cull;
    GLuint m_program_texture;
    GLuint m_program_rasterizer;
    GLuint m_grid_texture;

    GLuint m_vao_triangles;

    int m_frame;
    GLuint m_time_query[MAX_FRAMES];
    GLuint m_vertex_query[MAX_FRAMES];
    GLuint m_fragment_query[MAX_FRAMES];
    GLuint m_bench1_query[MAX_FRAMES];
    GLuint m_bench2_query[MAX_FRAMES];
    GLuint m_bench3_query[MAX_FRAMES];
    GLuint m_bench4_query[MAX_FRAMES];
};

int main( int argc, char **argv )
{
    Bench app;
    app.run();
}
