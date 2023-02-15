
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


struct Bench : public AppCamera
{
    Bench( ) : AppCamera(1024, 512, 4, 3)
    {
        vsync_off();
    }
    
    int init( )
    {
        //~ m_mesh= read_mesh_fast("data/robot.obj");
        //~ m_mesh= read_indexed_mesh_fast("/home/jciehl/scenes/sponza-intel/export.obj");
        m_mesh= read_mesh_fast("/home/jciehl/scenes/sponza-intel/export.obj");
        
        Point pmin, pmax;
        m_mesh.bounds(pmin, pmax);
        camera().lookat(pmin, pmax);
        
        m_grid_texture= read_texture(0, "data/grid.png");
        
        m_program_texture= read_program("tutos/bench/vertex2.glsl");
        program_print_errors(m_program_texture);
        
        m_program_rasterizer= read_program("tutos/bench/rasterizer.glsl");
        program_print_errors(m_program_rasterizer);

        if(program_errors(m_program_texture) || program_errors(m_program_rasterizer))
            return -1;
        
        // bench 1 : triangles mal orientes / cull rate
        m_triangles.create(GL_TRIANGLES);
        for(int i= 0; i < 1024*1024; i++)
        {
            // back face culled
            m_triangles.texcoord(0, 0);
            m_triangles.normal(0, 0, 1);
            m_triangles.vertex(-1, -1, 0);
            
            m_triangles.texcoord(1, 1);
            m_triangles.normal(0, 0, 1);
            m_triangles.vertex( 1,  1, 0);
            
            m_triangles.texcoord(1, 0);
            m_triangles.normal(0, 0, 1);
            m_triangles.vertex( 1, -1, 0);
        }
        
        m_vao_triangles= m_triangles.create_buffers(/* use_texcoord */ true, /* use_normal */ true, /* use_color */ false, /* use_material_index */ false);
        
        //
        glGenQueries(1, &m_time_query);
        glGenQueries(1, &m_bench1_query);
        glGenQueries(1, &m_bench2_query);
        glGenQueries(1, &m_bench3_query);
        glGenQueries(1, &m_bench4_query);
        
        // initialise les requetes... simplifie la collecte sur la 1ere frame...
        glBeginQuery(GL_TIME_ELAPSED, m_time_query); glEndQuery(GL_TIME_ELAPSED);
        glBeginQuery(GL_TIME_ELAPSED, m_bench1_query); glEndQuery(GL_TIME_ELAPSED);
        glBeginQuery(GL_TIME_ELAPSED, m_bench2_query); glEndQuery(GL_TIME_ELAPSED);
        glBeginQuery(GL_TIME_ELAPSED, m_bench3_query); glEndQuery(GL_TIME_ELAPSED);
        glBeginQuery(GL_TIME_ELAPSED, m_bench4_query); glEndQuery(GL_TIME_ELAPSED);
        
        // etat openGL par defaut
        glClearColor(0.2f, 0.2f, 0.2f, 1.f);
        
        glClearDepth(1.f);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_DEPTH_TEST);
        
        glFrontFace(GL_CCW);
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        
        return 0;
    }
    
    int quit( )
    {
        FILE *out= fopen("bench3.txt", "wt");
        if(out)
        {
            for(auto& stats : m_stats)
            {
                fprintf(out, "%f %f %f %f\n", 
                    stats.draw_time,                                                                   // 1
                    stats.bench1_time, stats.bench2_time, stats.bench3_time);       // 2 3 4 
            }
            
            fclose(out);
        }
        
        m_mesh.release();
        release_program(m_program_texture);
        release_program(m_program_rasterizer);

        glDeleteTextures(1, &m_grid_texture);
        
        glDeleteQueries(1, &m_time_query);
        glDeleteQueries(1, &m_bench1_query);
        glDeleteQueries(1, &m_bench2_query);
        glDeleteQueries(1, &m_bench3_query);
        glDeleteQueries(1, &m_bench4_query);
        
        return 0;
    }
    
    int render( )
    {
        // collecte les requetes de la frame precedente...
        GLint64 gpu_draw= 0;
        glGetQueryObjecti64v(m_time_query, GL_QUERY_RESULT, &gpu_draw);
        
        GLint64 gpu_bench1_draw= 0;
        glGetQueryObjecti64v(m_bench1_query, GL_QUERY_RESULT, &gpu_bench1_draw);
        GLint64 gpu_bench2_draw= 0;
        glGetQueryObjecti64v(m_bench2_query, GL_QUERY_RESULT, &gpu_bench2_draw);
        GLint64 gpu_bench3_draw= 0;
        glGetQueryObjecti64v(m_bench3_query, GL_QUERY_RESULT, &gpu_bench3_draw);
        
        GLint64 gpu_bench4_draw= 0;
        glGetQueryObjecti64v(m_bench4_query, GL_QUERY_RESULT, &gpu_bench4_draw);
        
        printf("  %.2fus draw time = %.2fus vertex + %2.fus rasterizer\n", 
            float(gpu_draw) / 1000,
            float(gpu_bench1_draw) / 1000,
            float(gpu_bench2_draw) / 1000);
        
        m_stats.push_back({
            float(gpu_draw) / 1000, 
            float(gpu_bench1_draw) / 1000,
            float(gpu_bench2_draw) / 1000,
            float(gpu_bench3_draw) / 1000,
            float(gpu_bench4_draw) / 1000 });
        
        //
        Transform model= RotationY(global_time() / 60);
        Transform view= camera().view();
        Transform projection= camera().projection(window_width(), window_height(), 45);
        Transform mv= view * model;
        Transform mvp= projection * mv;

    #if 0
        // en deriner, pour avoir qqchose de dessine...
        // test 1 : normal
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(m_program_texture);
        program_uniform(m_program_texture, "mvpMatrix", mvp);
        program_uniform(m_program_texture, "mvMatrix", mv);
        program_use_texture(m_program_texture, "grid", 0, m_grid_texture);
        
        glBeginQuery(GL_TIME_ELAPSED, m_time_query);
            
            m_mesh.draw(m_program_texture, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false, /* material */ false );
        
        glEndQuery(GL_TIME_ELAPSED);
    #endif
    
        // bench 1 : que les triangles
    #if 0
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_RASTERIZER_DISCARD);
        
        glUseProgram(m_program_texture);
        program_uniform(m_program_texture, "mvpMatrix", mvp);
        program_uniform(m_program_texture, "mvMatrix", mv);
        program_use_texture(m_program_texture, "grid", 0, m_grid_texture);
        
        glBeginQuery(GL_TIME_ELAPSED, m_bench1_query);
            
            m_mesh.draw(m_program_texture, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false, /* material */ false );
        
        glEndQuery(GL_TIME_ELAPSED);
        
        glDisable(GL_RASTERIZER_DISCARD);
        // pas efficace sur les geforces, temps equivalent au draw normal...
    #else
    
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glBindVertexArray(m_vao_triangles);
        glUseProgram(m_program_texture);
        program_uniform(m_program_texture, "mvpMatrix", Identity());
        program_uniform(m_program_texture, "mvMatrix", Identity());
        program_use_texture(m_program_texture, "grid", 0, m_grid_texture);
        
        {
            int instances= m_mesh.triangle_count() / m_triangles.triangle_count();
            int n= m_mesh.triangle_count() % m_triangles.triangle_count();
            if(instances == 0 && n == 0) n= 1;
            
            glBeginQuery(GL_TIME_ELAPSED, m_bench1_query);
                if(instances > 0)
                    glDrawArraysInstanced(GL_TRIANGLES, 0, m_triangles.triangle_count()*3, instances);
                if(n > 0)
                    glDrawArrays(GL_TRIANGLES, 0, n*3);
            glEndQuery(GL_TIME_ELAPSED);
        }
    #endif
    
        // bench 2 : que le rasterizer, pas les fragments
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDepthFunc(GL_LESS);
        glEnable(GL_DEPTH_TEST);
        
        glUseProgram(m_program_rasterizer);
        program_uniform(m_program_rasterizer, "mvpMatrix", mvp);
        
        glBeginQuery(GL_TIME_ELAPSED, m_bench2_query);
            
            m_mesh.draw(m_program_rasterizer, /* use position */ true, /* use texcoord */ false, /* use normal */ false, /* use color */ false, /* material */ false );
        
        glEndQuery(GL_TIME_ELAPSED);
        
        // bench 3 : que les fragments visibles ??
        
        
        // test 1 : normal
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(m_program_texture);
        program_uniform(m_program_texture, "mvpMatrix", mvp);
        program_uniform(m_program_texture, "mvMatrix", mv);
        program_use_texture(m_program_texture, "grid", 0, m_grid_texture);
        
        glBeginQuery(GL_TIME_ELAPSED, m_time_query);
            
            m_mesh.draw(m_program_texture, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false, /* material */ false );
        
        glEndQuery(GL_TIME_ELAPSED);
        
        return 1;   // on continue
    }
    
protected:
    std::vector<stats> m_stats;

    Mesh m_mesh;
    Mesh m_triangles;
    GLuint m_program_texture;
    GLuint m_program_rasterizer;
    GLuint m_grid_texture;

    GLuint m_vao_triangles;

    GLuint m_time_query;
    GLuint m_bench1_query;
    GLuint m_bench2_query;
    GLuint m_bench3_query;
    GLuint m_bench4_query;
};

int main( int argc, char **argv )
{
    Bench app;
    app.run();
}
