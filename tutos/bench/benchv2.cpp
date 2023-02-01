
#include "app.h"
#include "app_time.h"
#include "app_camera.h"

#include "vec.h"
#include "mat.h"

#include "program.h"
#include "uniforms.h"
#include "texture.h"

#include "mesh.h"
#include "wavefront.h"
#include "wavefront_fast.h"

#include "orbiter.h"

class Bench : public AppCamera
{
public:
    // application openGL 4.3
    Bench( ) : AppCamera(1024, 512,  4, 3) {}
    //~ Bench( ) : AppTime(2048, 1024,  4, 3) {}
    
    int init( )
    {
        m_mesh= read_mesh_fast("/home/jciehl/scenes/rungholt/rungholt.obj");
        //~ m_mesh= read_mesh_fast("data/bigguy.obj");
        
        Point pmin, pmax;
        m_mesh.bounds(pmin, pmax);
        //~ m_camera.lookat(pmin, pmax);
        camera().lookat(pmin, pmax);
        
        // cree la texture, 1 canal, entiers 32bits non signes
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0,
            GL_R32UI, window_width(), window_height(), 0,
            GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);  // GL_RED_INTEGER, sinon normalisation implicite...
        
        // pas la peine de construire les mipmaps / pas possible pour une texture int / uint
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        
        // fragments par triangle
        glGenBuffers(1, &m_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned) * m_mesh.triangle_count(), nullptr, GL_STATIC_COPY);
        
        // 
        m_program= read_program("tutos/bench/bench.glsl");
        program_print_errors(m_program);
        
        m_program_prepass= read_program("tutos/bench/prepass.glsl");
        program_print_errors(m_program_prepass);
        
        m_program_display= read_program("tutos/bench/bench_display.glsl");
        program_print_errors(m_program_display);

        // 
        if(program_errors(m_program) || program_errors(m_program_prepass) || program_errors(m_program_display))
            return -1;
        
        //
        glGenQueries(1, &m_sample_query);
        glGenQueries(1, &m_fragment_query);
        glGenQueries(1, &m_vertex_query);
        glGenQueries(1, &m_culling_query);
        glGenQueries(1, &m_clipping_query);
        
        // etat openGL par defaut
        glClearColor(0.2f, 0.2f, 0.2f, 1.f);        // couleur par defaut de la fenetre
        
        glClearDepth(1.f);                          // profondeur par defaut
        glDepthFunc(GL_LEQUAL);                       // ztest, conserver l'intersection la plus proche de la m_camera
        glEnable(GL_DEPTH_TEST);                    // activer le ztest
        //~ glDisable(GL_DEPTH_TEST);
        
        glFrontFace(GL_CCW);
        glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        //~ glDisable(GL_CULL_FACE);
        
        return 0;   // ras, pas d'erreur
    }
    
    // destruction des objets de l'application
    int quit( )
    {
        m_mesh.release();
        release_program(m_program);
        release_program(m_program_display);
        glDeleteTextures(1, &m_texture);
        glDeleteQueries(1, &m_sample_query);
        glDeleteQueries(1, &m_fragment_query);
        glDeleteQueries(1, &m_vertex_query);
        glDeleteQueries(1, &m_culling_query);
        glDeleteQueries(1, &m_clipping_query);
        
        return 0;
    }
    
    int update( const float time, const float delta )
    {
        return 0;
    }
    
    // dessiner une nouvelle image
    int render( )
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // effacer la texture image / compteur, le shader ajoute 1 a chaque fragment dessine...
        GLuint zero= 0;
        glClearTexImage(m_texture, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
        
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_buffer);
        glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
        
    #if 0
        // recupere les mouvements de la souris
        int mx, my;
        unsigned int mb= SDL_GetRelativeMouseState(&mx, &my);
        int mousex, mousey;
        SDL_GetMouseState(&mousex, &mousey);
        
        // deplace la m_camera
        if(mb & SDL_BUTTON(1))
            m_camera.rotation(mx, my);      // tourne autour de l'objet
        else if(mb & SDL_BUTTON(3))
            m_camera.translation((float) mx / (float) window_width(), (float) my / (float) window_height()); // deplace le point de rotation
        else if(mb & SDL_BUTTON(2))
            m_camera.move(mx);           // approche / eloigne l'objet

        SDL_MouseWheelEvent wheel= wheel_event();
        if(wheel.y != 0)
        {
            clear_wheel_event();
            m_camera.move(8.f * wheel.y);  // approche / eloigne l'objet
        }
    #endif
    
        if(key_state('r'))
        {
            clear_key_state('r');
            reload_program(m_program, "tutos/bench/bench.glsl");
            program_print_errors(m_program);
            
            reload_program(m_program_display, "tutos/bench/bench_display.glsl");
            program_print_errors(m_program_display);            
        }
        
        
        Transform model= RotationY(global_time() / 60);
        //~ Transform view= m_camera.view();
        //~ Transform projection= m_camera.projection(window_width(), window_height(), 45);
        Transform view= camera().view();
        Transform projection= camera().projection(window_width(), window_height(), 45);
        Transform mv= view * model;
        Transform mvp= projection * mv;
        
        // passe 0 : prepass
        if(key_state('z'))
        {
            glUseProgram(m_program_prepass);
            program_uniform(m_program_prepass, "mvpMatrix", mvp);
            m_mesh.draw(m_program_prepass, /* use position */ true, /* use texcoord */ false, /* use normal */ false, /* use color */ false, /* material */ false );
            
            //~ return 1;
        }
        
        // passe 1 : compter le nombre de fragments par pixel
        glUseProgram(m_program);
        program_uniform(m_program, "mvpMatrix", mvp);
        program_uniform(m_program, "mvMatrix", mv);
        
        // selectionne la texture sur l'unite image 0, operations atomiques / lecture + ecriture
        glBindImageTexture(0, m_texture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
        program_uniform(m_program, "image", 0);
        
        glBeginQuery(GL_SAMPLES_PASSED, m_sample_query);
        glBeginQuery(GL_FRAGMENT_SHADER_INVOCATIONS_ARB, m_fragment_query);
        glBeginQuery(GL_VERTEX_SHADER_INVOCATIONS_ARB, m_vertex_query);
        glBeginQuery(GL_CLIPPING_INPUT_PRIMITIVES_ARB, m_culling_query);         // reussi le test de front/back face culling
        glBeginQuery(GL_CLIPPING_OUTPUT_PRIMITIVES_ARB, m_clipping_query);
        {
            m_mesh.draw(m_program, /* use position */ true, /* use texcoord */ false, /* use normal */ false, /* use color */ false, /* material */ false );
        }
        glEndQuery(GL_SAMPLES_PASSED);
        glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS_ARB);
        glEndQuery(GL_VERTEX_SHADER_INVOCATIONS_ARB);
        glEndQuery(GL_CLIPPING_INPUT_PRIMITIVES_ARB);         // reussi le test de front/back face culling
        glEndQuery(GL_CLIPPING_OUTPUT_PRIMITIVES_ARB);
        
        if(key_state(' ') == 0)
        {
            // passe 2 : afficher le compteur
            glUseProgram(m_program_display);
            
            // attendre que les resultats de la passe 1 soit disponible
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            
            // RE-selectionne la texture sur l'unite image 0 / LECTURE SEULE
            glBindImageTexture(0, m_texture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32UI);
            program_uniform(m_program_display, "image", 0);
            
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        
    #if 1
        // recuperer le buffer
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        
        std::vector<unsigned> tmp(m_mesh.triangle_count());
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, tmp.size() * sizeof(unsigned), tmp.data());
        
        // compte les triangles reelement rasterizes, ie avec des fragments... 
        size_t fragments= 0;
        unsigned triangles_count= 0;
        for(unsigned i= 0; i < tmp.size(); i++)
            if(tmp[i] > 0)
            {
                triangles_count++;
                fragments+= tmp[i];
            }
        
        printf("%u rasterized triangles / %d\n", triangles_count, m_mesh.triangle_count());
        printf("%lu fragments / %d pixels, %f overdraw\n", fragments, window_width() * window_height(), 
            double(fragments) / double(window_width() * window_height()));
        printf("%.2f pixels / triangle\n", double(fragments) / double(triangles_count));
    #endif
    
        //
        GLint64 gpu_samples= 0;
        glGetQueryObjecti64v(m_sample_query, GL_QUERY_RESULT, &gpu_samples);
        printf("  %lu %.2fM samples\n", gpu_samples, double(gpu_samples) / 1000000.0);
        printf("  %.2f overdraw / %d pixels\n", double(gpu_samples) / double(window_width()*window_height()), window_width()*window_height());
        
        //~ // estime ? le debit memoire sur le zbuffer
        //~ printf("    %.2fMB zbuffer always\n", double(gpu_samples*8) / double(1024*1024));
        //~ printf("    %.2fMB zbuffer less\n", double(gpu_samples*12) / double(1024*1024));
        
        //
        GLint64 gpu_fragments= 0;
        glGetQueryObjecti64v(m_fragment_query, GL_QUERY_RESULT, &gpu_fragments);
        printf("  %lu %.2fM fragments\n", gpu_fragments, double(gpu_fragments) / 1000000.0);
        printf("  %.2f overshade\n", double(gpu_fragments) / double(gpu_samples));

        // estime ? le debit memoire sur le zbuffer
        printf("    %.2fMB zbuffer always\n", double(gpu_fragments*8) / double(1024*1024));
        printf("    %.2fMB zbuffer less\n", double(gpu_fragments*12) / double(1024*1024));

        //
        GLint64 gpu_vertices= 0;
        glGetQueryObjecti64v(m_vertex_query, GL_QUERY_RESULT, &gpu_vertices);
        printf("  %lu %.2fM vertices\n", gpu_vertices, double(gpu_vertices) / 1000000.0);
        printf("  vertex bw %.2f MB\n", double(gpu_vertices*sizeof(float)*8) / double(1024*1024));
        printf("  overtransform x%.2f\n", double(m_mesh.triangle_count()*3) / double(gpu_vertices));
        
        //
        GLint64 gpu_culling= 0;
        glGetQueryObjecti64v(m_culling_query, GL_QUERY_RESULT, &gpu_culling);
        printf("  %lu !culled triangles / %d triangles\n", gpu_culling, m_mesh.triangle_count());
        
        GLint64 gpu_clipping= 0;
        glGetQueryObjecti64v(m_clipping_query, GL_QUERY_RESULT, &gpu_clipping);
        printf("  %lu !clipped triangles / %d triangles\n", gpu_clipping, m_mesh.triangle_count());
        printf("  %.2f pixels / triangle\n", double(gpu_samples) / double(gpu_clipping));
        
        printf("\n");
        
        //~ static int video= 0;
        //~ if(key_state('v'))
        //~ {
            //~ clear_key_state('v');
            //~ video= (video +1)%2;
            
            //~ if(video) printf("[REC]");
            //~ else printf("[REC] off");
        //~ }
        
        //~ if(video) 
            //~ capture("bench");
        
        return 1;
    }

protected:
    Mesh m_mesh;
    Orbiter m_camera;

    GLuint m_program;
    GLuint m_program_prepass;
    GLuint m_program_display;
    GLuint m_texture;
    
    GLuint m_buffer;
    GLuint m_sample_query;
    GLuint m_fragment_query;
    GLuint m_vertex_query;
    GLuint m_culling_query;
    GLuint m_clipping_query;
};


int main( int argc, char **argv )
{
    Bench app;
    app.run();
    
    return 0;
}
