
//! \file tuto_storage_texture.cpp  exemple direct d'utilisation d'une storage texture / image.  le fragment shader compte combien de fragments sont calcules par pixel.

#include "app_time.h"

#include "vec.h"
#include "mat.h"

#include "program.h"
#include "uniforms.h"
#include "texture.h"

#include "mesh.h"
#include "wavefront.h"
#include "wavefront_fast.h"

#include "orbiter.h"


class StorageImage : public AppTime
{
public:
    // application openGL 4.3
    StorageImage( ) : AppTime(1024, 640,  4, 3) {}
    
    int init( )
    {        
        m_program= read_program("tutos/bench/setup.glsl");
        program_print_errors(m_program);
        
        m_program_display= read_program("tutos/bench/setup_display.glsl");
        program_print_errors(m_program_display);

        if(program_errors(m_program) || program_errors(m_program_display))
            return -1;
        
        //~ m_mesh= read_mesh_fast("/home/jciehl/scenes/bistro/interior.obj");
        //~ m_mesh= read_mesh_fast("/home/jciehl/scenes/bistro/exterior.obj");
        //~ m_mesh= read_mesh_fast("/home/jciehl/scenes/rungholt/rungholt.obj");
        //~ m_mesh= read_mesh_fast("data/cube.obj");
        m_mesh= read_mesh_fast("data/bigguy.obj");
        
        Point pmin, pmax;
        m_mesh.bounds(pmin, pmax);
        m_camera.lookat(pmin, pmax);
        
        // cree la texture, 1 canal, entiers 32bits non signes
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0,
            GL_R32UI, window_width(), window_height(), 0,
            GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);  // GL_RED_INTEGER, sinon normalisation implicite...
        
        // pas la peine de construire les mipmaps / pas possible pour une texture int / uint
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        
        // blocs par triangle
        glGenBuffers(1, &m_tile_buffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_tile_buffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned) * m_mesh.triangle_count(), nullptr, GL_STATIC_COPY);
        
        // etat openGL par defaut
        glClearColor(0.2f, 0.2f, 0.2f, 1.f);        // couleur par defaut de la fenetre
        
        glClearDepth(1.f);                          // profondeur par defaut
        glDepthFunc(GL_LEQUAL);                       // ztest, conserver l'intersection la plus proche de la m_camera
        //~ glEnable(GL_DEPTH_TEST);                    // activer le ztest
        glDisable(GL_DEPTH_TEST);                    // activer le ztest
        
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
        glDeleteTextures(1, &m_texture);
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

        if(key_state('r'))
        {
            clear_key_state('r');
            reload_program(m_program, "tutos/bench/setup.glsl");
            program_print_errors(m_program);
            
            reload_program(m_program_display, "tutos/bench/setup_display.glsl");
            program_print_errors(m_program_display);            
        }
        
        static int time= 1;
        static Transform last_model= Identity(); 
        
        if(key_state('t'))
        {
            clear_key_state('t');
            time= (time+1) %2;
        }
        
        Transform model;
        if(time)
        {
            model= RotationY(global_time() / 60);
            last_model= model;
        }
        else
        {
            model= last_model;
        }
        //~ Transform model= RotationY(global_time() / 60);
        
        Transform view= m_camera.view();
        Transform projection= m_camera.projection(window_width(), window_height(), 45);
        Transform mv= view * model;
        Transform mvp= projection * mv;
        
    // etape 1 : compter le nombre de fragments par pixel
        glUseProgram(m_program);
        program_uniform(m_program, "mvpMatrix", mvp);
        
        // selectionne la texture sur l'unite image 0, operations atomiques / lecture + ecriture
        glBindImageTexture(0, m_texture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
        program_uniform(m_program, "image", 0);
        
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_tile_buffer);
        glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
        
        // indiquer quels attributs de sommets du mesh sont necessaires a l'execution du shader.
        // le shader n'utilise que position. les autres de servent a rien.
        m_mesh.draw(m_program, /* use position */ true, /* use texcoord */ false, /* use normal */ false, /* use color */ false, /* material */ false );
        
        if(key_state(' ') == 0)
        {
            // passe 2 : afficher le compteur
            glUseProgram(m_program_display);

            // attendre que les resultats de la passe 1 soit disponible
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
            
            // RE-selectionne la texture sur l'unite image 0 / LECTURE SEULE
            //~ glBindImageTexture(0, m_texture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32UI);
            program_uniform(m_program_display, "image", 0);
            
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        
    // etape 2 : recupere le buffer
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        
        std::vector<unsigned> tmp(m_mesh.triangle_count());
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, tmp.size() * sizeof(unsigned), tmp.data());
        
        // compte les triangles reellement rasterizes, ie avec des fragments... 
        unsigned tiles= 0;
        unsigned n= 0;
        for(unsigned i= 0; i < tmp.size(); i++)
        {
            if(tmp[i] > 0)
            {
                n++;
                tiles+= tmp[i];
            }
        }
        
        printf("%u rasterized triangles\n", n);
        printf("%u tiles, %.2f tiles/triangle\n", tiles, float(tiles)/float(n));
        
        return 1;
    }

protected:
    Mesh m_mesh;
    Orbiter m_camera;

    GLuint m_program;
    GLuint m_program_display;
    GLuint m_texture;
    GLuint m_tile_buffer;
};


int main( int argc, char **argv )
{
    StorageImage app;
    app.run();
    
    return 0;
}
