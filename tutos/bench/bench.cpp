#include "vec.h"
#include "program.h"
#include "uniforms.h"

#include "mesh.h"
#include "texture.h"

#include "app.h"        // classe Application a deriver

struct stat
{
    int vertex;
    int triangle;
    size_t fragments;
    float vertex_buffer_size;  // Mo
    float colorz_buffer_size;
    float vertex_buffer_bw;     // Mo/s
    float colorz_buffer_bw;
    float time; // us 
    float vertex_rate;
    float fragment_rate;
};


class TP : public App
{
public:
    // constructeur : donner les dimensions de l'image, et eventuellement la version d'openGL.
    TP( std::vector<const  char *> options ) : App(512, 512)
    {
        bool culled= false;
        const char *filename= "bench.txt";
        
        for(unsigned i= 1; i < options.size(); i++)
        {
            if(std::string(options[i]) == "--cull" || std::string(options[i]) == "--culled")
                culled= true;
            if(std::string(options[i]) == "--fill" || std::string(options[i]) == "--filled")
                culled= false;
            if(options[i][0] == '-')
                if(std::string(options[i]) == "-o" && i+1 < options.size())
                    filename= options[i+1];
        }
        
        m_culled= culled;
        m_filename= filename;
    }
    
    // creation des objets de l'application
    int init( )
    {
        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);
        
        // genere quelques triangles...
        m_mesh= Mesh(GL_TRIANGLES);
        for(int i= 0; i < 1024*1024; i++)
        {
            m_mesh.texcoord(0, 0);
            m_mesh.normal(0, 0, 1);
            m_mesh.vertex(-1, -1, 0);
            
            m_mesh.texcoord(1, 0);
            m_mesh.normal(0, 0, 1);
            m_mesh.vertex( 1, -1, 0);

            m_mesh.texcoord(1, 1);
            m_mesh.normal(0, 0, 1);
            m_mesh.vertex( 1,  1, 0);
        }
        
        m_grid_texture= read_texture(0, "data/grid.png");
        
        //~ m_program=read_program("tutos/bench/vertex1.glsl");
        m_program=read_program("tutos/bench/vertex2.glsl");
        program_print_errors(m_program);
        
        // requete pour mesurer le temps gpu
        glGenQueries(1, &m_query);
        glGenQueries(32, m_time_queries);
        glGenQueries(1, &m_sample_query);
        glGenQueries(1, &m_fragment_query);
        glGenQueries(1, &m_vertex_query);
        
        // etat openGL par defaut
        glClearColor(0.2f, 0.2f, 0.2f, 1.f);        // couleur par defaut de la fenetre
        
        glClearDepth(1.f);                          // profondeur par defaut
        glDepthFunc(GL_LEQUAL);                       // ztest, conserver l'intersection la plus proche de la camera
        //~ glDepthFunc(GL_ALWAYS);                       // ztest, conserver l'intersection la plus proche de la camera
        glEnable(GL_DEPTH_TEST);                    // activer le ztest
        //~ glDisable(GL_DEPTH_TEST);                    // activer le ztest
        
        glFrontFace(GL_CCW);
        if(m_culled)
            glCullFace(GL_FRONT);
        else
            glCullFace(GL_BACK);
        glEnable(GL_CULL_FACE);
        
        // transfere les donnees...
        glUseProgram(m_program);
        program_uniform(m_program, "mvp", Identity());
        program_uniform(m_program, "mv", Identity());
        program_use_texture(m_program, "grid", 0, m_grid_texture);
        
        m_mesh.draw(0, 3, m_program, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false,  /* use material index */ false);
        
        return 0;   // ras, pas d'erreur
    }
    
    // destruction des objets de l'application
    int quit( )
    {
        // exporte les mesures
        {
            printf("writing '%s'...\n", m_filename);
            FILE *out= fopen(m_filename, "wt");
            if(out)
            {
                for(unsigned i= 0; i < m_stats.size(); i++)
                    fprintf(out, "%d %d %lu %f %f %f %f %f %f %f\n", 
                        m_stats[i].vertex, m_stats[i].triangle, m_stats[i].fragments,       // 1 2 3
                        m_stats[i].vertex_buffer_size, m_stats[i].colorz_buffer_size,       // 4 5
                        m_stats[i].vertex_buffer_bw, m_stats[i].colorz_buffer_bw,           // 6 7
                        m_stats[i].time,                                                    // 8
                        m_stats[i].vertex_rate, m_stats[i].fragment_rate);                  // 9 10
                
                fclose(out);
            }
        }
        
        release_program(m_program);
        m_mesh.release();
        glDeleteQueries(1, &m_query);
        glDeleteQueries(1, &m_sample_query);
        glDeleteQueries(1, &m_fragment_query);
        glDeleteQueries(1, &m_vertex_query);        
        return 0;
    }
    
    // dessiner une nouvelle image
    int render( )
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glUseProgram(m_program);
        program_uniform(m_program, "mvp", Identity());
        program_uniform(m_program, "mv", Identity());
        program_use_texture(m_program, "grid", 0, m_grid_texture);
        
        // calibre le test pour occuper le gpu / stabiliser les mesures / frequences...
        static int busy_n= 16;
        static int busy_loop= 1;
        static int busy_mode= 0;
        if(busy_mode == 0)
        {
            double ms= 0;
            while(ms < 8)
            {
                if(busy_n*2 > m_mesh.triangle_count())
                    busy_loop++;
                else
                    busy_n= busy_n*2;
                
                glBeginQuery(GL_TIME_ELAPSED, m_query);
                    //~ glDrawArrays(GL_TRIANGLES, 0, busy_n*3);
                    for(int i= 0; i < busy_loop; i++)
                        m_mesh.draw(0, busy_n*3, m_program, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false,  /* use material index */ false);
                    
                glEndQuery(GL_TIME_ELAPSED);

                GLint64 test_time= 0;
                glGetQueryObjecti64v(m_query, GL_QUERY_RESULT, &test_time);
                ms= double(test_time) / double(1000000);
                printf("busy %d*%d= %d %.2fms\n", busy_n, busy_loop, busy_n * busy_loop, ms);
            }
            
            // calibration ok, maintenant on mesure
            busy_mode= 1;
            return 1;
        }

        // stabilise le gpu
        //~ glDrawArrays(GL_TRIANGLES, 0, busy_n*3);
        for(int i= 0; i < busy_loop; i++)
            m_mesh.draw(0, busy_n*3, m_program, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false,  /* use material index */ false);
        
        // test
        static int n= 4;
        
        // mesure le temps d'execution du draw pour le gpu
        //~ glBeginQuery(GL_TIME_ELAPSED, m_query);
        glBeginQuery(GL_SAMPLES_PASSED, m_sample_query);
        glBeginQuery(GL_FRAGMENT_SHADER_INVOCATIONS_ARB, m_fragment_query);
        glBeginQuery(GL_VERTEX_SHADER_INVOCATIONS_ARB, m_vertex_query);
        {
            //~ glDrawArrays(GL_TRIANGLES, 0, n*3);
            m_mesh.draw(0, n*3, m_program, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false,  /* use material index */ false);
        }
        //~ glEndQuery(GL_TIME_ELAPSED);
        glEndQuery(GL_SAMPLES_PASSED);
        glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS_ARB);
        glEndQuery(GL_VERTEX_SHADER_INVOCATIONS_ARB);

#if 0        
        glBeginQuery(GL_TIME_ELAPSED, m_query);
            m_mesh.draw(0, busy_n*3, m_program, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false,  /* use material index */ false);
        glEndQuery(GL_TIME_ELAPSED);
        
        // attendre le resultat de la requete
        GLint64 gpu_time= 0;
        glGetQueryObjecti64v(m_query, GL_QUERY_RESULT, &gpu_time);
        double time= double(gpu_time) / double(busy_n) * double(n) / double(1000000000);
        //~ double time= double(gpu_time) / double(128) * double(n) / double(1000000000);
        //~ double time= double(gpu_time) / double(1000000000);
#else
        // moyenner plusieurs realisations...
        for(int i= 0; i < 32; i++)
        {
            glBeginQuery(GL_TIME_ELAPSED, m_time_queries[i]);
                //~ m_mesh.draw(0, busy_n*3, m_program, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false,  /* use material index */ false);
                m_mesh.draw(0, n*3, m_program, /* use position */ true, /* use texcoord */ true, /* use normal */ true, /* use color */ false,  /* use material index */ false);
            glEndQuery(GL_TIME_ELAPSED);
        }
        
        // attendre le resultat des requetes
        double time= 0;
        for(int i= 0; i < 32; i++)
        {
            GLint64 gpu_time= 0;
            glGetQueryObjecti64v(m_time_queries[i], GL_QUERY_RESULT, &gpu_time);
            time+= double(gpu_time) / double(1000000000);
        }
        //~ time= time / 32 * double(n) / double(busy_n);
        time= time / 32;
#endif
        
        //~ printf("n %lu gpu  %02dms %03dus %03dns\n", n, int(gpu_time / 1000000), int((gpu_time / 1000) % 1000), int(gpu_time) % 1000);
        printf("%d %.2fms\n", n, time * 1000);
        
        printf("  %.2f triangles/s\n", n / time);
        
        //
        GLint64 gpu_samples= 0;
        glGetQueryObjecti64v(m_sample_query, GL_QUERY_RESULT, &gpu_samples);
        printf("  %lu %.2fM samples/s\n", gpu_samples, gpu_samples / 1000000.0 / time );
        
        // estime ? le debit memoire sur le zbuffer
        printf("    %luMB zbuffer always, %.2fMB/s\n", gpu_samples*8/1024/1024, gpu_samples*8/1024/1024 / time);
        printf("    %luMB zbuffer less, %.2fMB/s\n", gpu_samples*12/1024/1024, gpu_samples*12/1024/1024 / time);
        
        //
        GLint64 gpu_fragments= 0;
        glGetQueryObjecti64v(m_fragment_query, GL_QUERY_RESULT, &gpu_fragments);
        printf("  %lu %.2fM fragments/s\n", gpu_fragments, gpu_fragments / 1000000.0 / time );
        
        if(gpu_samples != gpu_fragments)
            // bug sur le compteur de fragments, pas trop sur 64bits... 
            return 0;
        
        //
        GLint64 gpu_vertices= 0;
        glGetQueryObjecti64v(m_vertex_query, GL_QUERY_RESULT, &gpu_vertices);
        printf("  %lu %.2fM vertices/s\n", gpu_vertices, gpu_vertices / 1000000.0 / time );
        printf("  %.2f MB/s\n", gpu_vertices*sizeof(float)*8 / time / (1024.0*1024.0));
        printf("  x%.2f\n", double(n*3) / double(gpu_vertices));

        //~ struct stat
        //~ {
                //~ int vertex;
                //~ int triangle;
                //~ size_t fragments;
                //~ float vertex_buffer_size;  // Mo
                //~ float colorz_buffer_size;
                //~ float vertex_buffer_bw;     // Mo/s
                //~ float colorz_buffer_bw;
                //~ float time; // us 
                //~ float vertex_rate;
                //~ float fragment_rate;
        //~ };        
        m_stats.push_back( { 
                int(gpu_vertices), n, size_t(gpu_fragments),
                float(gpu_vertices*sizeof(float)*8) / float(1024*1024), float(gpu_samples *8) / float(1024*1024), 
                float(gpu_vertices*sizeof(float)*8 / time / (1024*1024)), float(gpu_samples*8 / time / (1024*1024)),
                float(time * float(1000000)),
                float(n*3 / time),
                float(gpu_fragments / time)
            } );
        
        if(n < busy_n)
            //~ n= n +256;
            n= n *2;
        else
            return 0;
            
        return 1;
    }

protected:
    const char *m_filename;
    bool m_culled;
    Mesh m_mesh;
    GLuint m_grid_texture;
    GLuint m_vao;
    GLuint m_program;
    GLuint m_query;
    GLuint m_time_queries[32];
    GLuint m_sample_query;
    GLuint m_fragment_query;
    GLuint m_vertex_query;
    
    std::vector<stat> m_stats;
};


int main( int argc, char **argv )
{
    // il ne reste plus qu'a creer un objet application et la lancer 
    TP tp(std::vector<const char *>( argv, argv + argc ));
    tp.run();
    
    return 0;
}
