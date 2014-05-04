//SanLink
//san.link@yahoo.com.mx
//http://electrolinks.blogspot.com
//Programa que lee archivos JPG de cualquier slot,
//los descomprime y determina si caben en pantalla.
//Si cabe, lo centra en 640x480 y lo muestra.
//Si no cabe, lo reduce y lo muestra en pantalla.

//devkitppc r15
//libogc 20080602

//v0.17
//VIDEO PROGRESIVO DISPONIBLE SI HAY CABLES CONECTADOS
//v0.16
//NO MUESTRA MENSAJES DEBUG
//v0.15
//ALGORITMO DE REDUCCION DE IMAGEN MEJORADO
//v0.14
//ACTUALIZACION A R15 Y 20080602
//v0.13
//ENCUENTRA ARCHIVOS JPG O JPEG EN CARPETA SD:\JPEG
//LOS VA MOSTRANDO CON A y B
//v0.12
//MUESTRA PANTALLA DE BIENVENIDA
//v0.11
//PROGRAMA QUE YA REDUCE IMAGENES DE 1024x768 
//HAY QUE VER EL TAMAÑO MAXIMO QUE JPEGDecompress PERMITE (???)
//v0.1
//PROGRAMA QUE YA REDUCE IMAGENES DE 1024x768
//TODAVIA TIENE BUGS CON IMAGENES A REDUCIR EN ALTO

#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <ogcsys.h>
#include <string.h>
#include <malloc.h>
#include "freetype.h"
#include <string.h>
#include <fat.h>
#include <sys/dir.h>
#include <jpeg/jpgogc.h>
#include <unistd.h>

//////////////////////////////////////////////////////////////////////////////
//Carga imagen de fondo de bienvenida
//picture.s
extern char fondodata[];
extern int tam_fondo;

//////////////////////////////////////////////////////////////////////////////
/*** 2D Video Globals ***/
GXRModeObj     *vmode;
u32            *xfb[2] = { NULL, NULL };
int             whichfb = 0;
int screenheight;
unsigned int progresivo; //indica si el cable componente está presente
unsigned int modo_video; //modo de video del gc

//////////////////////////////////////////////////////////////////////////////
//Variables de manejo de archivos
char filename[MAXPATHLEN]; //nombre de archivo
FILE* handle; //handle
int conteo=0;
int i=0;
char lArchivo[1024][MAXPATHLEN]; //1024 archivos
char temporal[MAXPATHLEN];
DIR_ITER* dir;
bool ok;

//////////////////////////////////////////////////////////////////////////////
//Video initialise
static void Initialise(void)
{
  VIDEO_Init ();
  PAD_Init ();

//Verifica si el cable componente está conectado
progresivo=VIDEO_HaveComponentCable();
if(progresivo)
    {
    modo_video=VIDEO_GetCurrentTvMode();
    switch (modo_video)
        {
        case VI_NTSC:
          vmode = &TVNtsc480Prog; //480 lines,progressive,singlefield NTSC mode
          break;
        case VI_PAL:
          vmode = &TVPal524IntAa; //524 lines,interlaced,antialiased PAL mode
          break;
        case VI_MPAL:
          vmode = &TVMpal480IntDf; //480 lines,interlaced,doublefield,antialiased PAL mode
          break;
        default:
          vmode = &TVNtsc480Prog; //480 lines,progressive,singlefield NTSC mode
          break;
        }
    }
else //no hay cable componente
    {
    modo_video=VIDEO_GetCurrentTvMode();
    switch (modo_video)
        {
        case VI_NTSC:
          vmode = &TVNtsc480IntAa; //480 lines,interlaced,doublefield,antialiased NTSC mode
          break;
        case VI_PAL:
          vmode = &TVPal524IntAa; //524 lines,interlaced,antialiased PAL mode
          break;
        case VI_MPAL:
          vmode = &TVMpal480IntDf; ////480 lines,interlaced,doublefield,antialiased PAL mode
          break;
        default:
          vmode = &TVNtsc480IntAa; //480 lines,interlaced,doublefield,antialiased NTSC mode
          break;
        }
    }

  VIDEO_Configure (vmode);
  xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
  xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (vmode));
  console_init (xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight,
		vmode->fbWidth * 2);
  screenheight = vmode->xfbHeight;
  VIDEO_ClearFrameBuffer (vmode, xfb[0], COLOR_BLACK);
  VIDEO_ClearFrameBuffer (vmode, xfb[1], COLOR_BLACK);
  VIDEO_SetNextFramebuffer (xfb[0]);
  VIDEO_SetPostRetraceCallback (PAD_ScanPads);
  VIDEO_SetBlack (0);
  VIDEO_Flush ();
  VIDEO_WaitVSync ();		/*** Wait for VBL ***/
  if (vmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync ();
}

/////////////////////////////////////////////////////////////////////
//Determina si presiona A o B
bool LeeAB()
{
int botones=0;
while(botones==0)
    botones = PAD_ButtonsDown(0)|PAD_ButtonsHeld(0);
if(botones&PAD_BUTTON_A)
    return 0;
if(botones&PAD_BUTTON_B)
    return 1;
return 0; //por si no apretó a ni b
}

/////////////////////////////////////////////////////////////////////
//Espera presionar A
void PressA()
{
while (!(PAD_ButtonsDown(0) & PAD_BUTTON_A));
sleep(1);
}

/////////////////////////////////////////////////////////////////////
//Imprime texto debug
void PrintA(int pos,char *texto)
{
setfontsize (20);
setfontcolour (255,0,0); //rojo
DrawText(250,pos-150,texto);
sleep(1);
}

/////////////////////////////////////////////////////////////////////
//Pasa xfb al buffer actual en pantalla
void RefrescaVideo()
{
//Selecciona el buffer de la imagen y trázalo
VIDEO_SetNextFramebuffer(xfb[whichfb]);
VIDEO_Flush();
VIDEO_WaitVSync();
return;
}

/////////////////////////////////////////////////////////////////////
//Función principal
int main(int argc, char **argv)
{
JPEGIMG jpeg,fondo;
int pix; //apunta a un par de pixeles en la salida de jpeg
unsigned int *jpegout,*fondo_out;
unsigned int piclength=0; //tamaño de picdata
char *picdata=""; //almacena el archivo jpeg
int i,j,k;
int primerpix; //es el apuntador al primer pixel de la imagen en
               //buffer xfb (320x480)
bool vga; //vga=1 imagen menor o igual a 640x480
          //vga=0 imagen mayor a 640x480
float r_aspecto; //relación de aspecto de la imagen a dibujar
float difancho,difalto; //la diferencia entre la imagen y 640x480
float escala; //la escala a usar para hacer chica la imagen
int n_ancho,n_alto; //nuevo ancho y alto de la imagen a achicar
float f_ancho,f_alto; //ancho y alto nuevos dados en flotante
unsigned int *jpeg_ycbcr; //la imagen original pero en ycbcr
unsigned int *jpegtemp; //la imagen escalada 1 pixel por int32
unsigned int *jpegnuevo; //la imagen escalada 2 pixeles por 1 int32
unsigned int temp,p_cb,p_cr,y_1,y_2;
int width,height;
float pix_float,k_float; //un puntero flotante para contar la escala
bool impar; //0 es par, 1 es impar
float paso_ancho,paso_alto; //avances para la reducción
int offset,row,col;
int arc_act; //apunta al archivo a trabajar
bool jpg_valido; //indica si el archivo tiene extensión
    //.jpg, .jpeg, .JPG, .JPEG
int cf,cc; //conteo fila y conteo columna

memset(&jpeg, 0, sizeof(JPEGIMG)); //pone a ceros la estructura jpegimg
memset(&fondo, 0, sizeof(JPEGIMG));

//Inicia video
Initialise();

//Inicia rutina de freetype
FT_Init();
ClearScreen ();

//Inicia librería fat
ok=fatInitDefault();

//Inicia la descompresión del fondo y lo guarda en un buffer
fondo.inbuffer = fondodata;
fondo.inbufferlength = tam_fondo;
JPEG_Decompress(&fondo);
fondo_out = (unsigned int *) fondo.outbuffer;
free(fondo.outbuffer);

//llena el buffer con la imagen
whichfb ^= 1;
pix = 0;
offset = 0;
for (row = 0; row < fondo.height; row++)
    {
    for (col = 0; col < (fondo.width >> 1); col++)
        xfb[whichfb][offset + col] = fondo_out[pix++];
    offset += 320;
    }

//indica en pantalla el modo de video usado
if(progresivo&&(modo_video==VI_NTSC))
    sprintf(temporal,"Video mode: 480p 60Hz");
if(progresivo&&(modo_video==VI_PAL))
    sprintf(temporal,"Video mode: 524i 50Hz");
if(progresivo&&(modo_video==VI_MPAL))
    sprintf(temporal,"Video mode: 480i 50Hz");
if(!progresivo&&(modo_video==VI_NTSC))
    sprintf(temporal,"Video mode: 480i 60Hz");
if(!progresivo&&(modo_video==VI_PAL))
    sprintf(temporal,"Video mode: 524i 50Hz");
if(!progresivo&&(modo_video==VI_MPAL))
    sprintf(temporal,"Video mode: 480i 50Hz");

//Presenta programa
setfontsize (30);
setfontcolour (253, 144, 77); //naranja
DrawText(50,60,"SL JPEG Viewer v0.17");
setfontsize (15);
setfontcolour (0,0,0); //negro
DrawText(50,80,"Developed with devkitPPC and libOGC,");
DrawText(50,100,"libjpeg, libfreetype and libfat.");
//modo de video actual
setfontcolour (1, 76, 189); //azul
DrawText(50,120,temporal);
setfontsize (20);
setfontcolour (1, 76, 189); //azul
DrawText(50,400,"Before running this dol, an SD card");
DrawText(50,425,"must be inserted. Press any button.");
//Dibuja
ShowScreen();

//espera a que presione a o b, recibiendo 0 ó 1
PressA();
DrawText(50,200,"Reading SD:/JPEG/");
if(ok==0) //fat init
    {
    DrawText(50,250,"Error mounting sd card.");
    while(1); //fin del programa
    }
    
//lee contenido de carpeta
dir=diropen("fat:/JPEG/");
if (dir == NULL) //Si está vacío
    {
    DrawText(50,250,"SD:/JPEG/ folder error.");
    while(1); //fin del programa
    }

//deposita en el arreglo lArchivo, la lista de jpeg encontrados
//conteo indica el total de archivos encontrados
while(dirnext(dir,filename,0)==0)
    {
    //verifica que el archivo sea un jpg válido
    //.jpg, .JPG
    jpg_valido=0;
    j=0;
    for(i=(strlen(filename)-4);i<strlen(filename);i++)
        temporal[j++]=filename[i];
    temporal[j]=0;
    if(strcmp(temporal,".jpg")==0) jpg_valido=1;
    if(strcmp(temporal,".JPG")==0) jpg_valido=1;
    //.jpeg, .JPEG
    j=0;
    for(i=(strlen(filename)-5);i<strlen(filename);i++)
        temporal[j++]=filename[i];
    temporal[j]=0;
    if(strcmp(temporal,".jpeg")==0) jpg_valido=1;
    if(strcmp(temporal,".JPEG")==0) jpg_valido=1;
    if(jpg_valido)
        {
        strcpy(lArchivo[conteo], filename);
        ++conteo;
        }
    }
dirclose(dir);

if(conteo==0) //no encontró jpeg
    {
    DrawText(50,250,"SD:/JPEG/ contains no jpeg files.");
    while(1); //fin del programa
    }

setfontsize (20);
setfontcolour (1, 76, 189); //azul
sprintf(temporal,"%d jpeg files found.",conteo);
DrawText(50,240,temporal);
sleep(1);

//Empieza el show infinito
while(1)
    {
    //archivo actual empieza en 2 para brincar . y ..
    for(arc_act=0;arc_act<conteo;arc_act++)
        {
        //debug
        //sprintf(temporal,"arc_act %d",arc_act);
        //PrintA(300,temporal);
        //arma ruta actual
        filename[0]=0;
        sprintf(filename,"fat:/JPEG/%s",lArchivo[arc_act]);
        //debug
        //PrintA(340,filename);

        //abre el archivo
        handle=fopen(filename,"r");
        if (handle <= 0)
            {
            setfontsize (20);
            setfontcolour (255,0,0); //rojo
            DrawText(50,240,"SD Card error (handle).");
            while(1);
            }
        //debug
        //PrintA(360,"handle ok");
        
        //verifica que no esté vacío
        fseek(handle,0,SEEK_END);
        piclength = ftell(handle);
        rewind(handle);
        if (piclength <= 0)
            {
            setfontsize (20);
            setfontcolour (255,0,0); //rojo
            DrawText(50,240,"Empty file (piclength).");
            sleep(3);
            goto next; //siguiente archivo
            }
            
        //ubica ram
        picdata=(char*)malloc(piclength);
        if(picdata<=0) //no pudo dar ram
            {
            PrintA(380,"Not enough RAM (picdata).");
            sleep(3);
            goto next; //siguiente archivo
            }
        //debug
        //sprintf(temporal,"picdata = %p",picdata);
        //PrintA(380,temporal);
            
        //deposita el archivo jpg en ram
        ok=fread(picdata,1,piclength,handle);
        if(ok==0)
            {
            sprintf(temporal,"Error opening %s.",filename);
            PrintA(380,temporal);
            sleep(3);
            goto next; //siguiente archivo
            }
        //PrintA(400,"Reading ok");
            
        //cierra el archivo
        fclose(handle);        

        ///////////////////////////////////////////////////////////////////////

        jpeg.inbuffer = picdata;
        jpeg.inbufferlength = piclength;

        JPEG_Decompress(&jpeg);
            
        ///////////////////////////////////////////////////////////////////////
        
        sprintf(temporal,"Width %d Height %d",jpeg.width,jpeg.height);
        DrawText(50,450,temporal);
        sleep(1);

        whichfb ^= 1; //cambia a la memoria de video siguiente
        pix = 0;

        jpegout = (unsigned int *) jpeg.outbuffer; //relaciona jpegout con el
            //contenido de outbuffer

        free(picdata); //limpia variable

        //llena el buffer xfb de pixeles azules (Fondo)
        //320*480=153600 
        for(i=0;i<153600;i++)
            xfb[whichfb][i]=0x9e929e3e;

        //determina si la imagen cabe en vga o no
        vga=!((jpeg.height>480)||(jpeg.width>640));

        //si la imagen cabe en 640x480 solo la centra y la dibuja tal cual
        if(vga)
            {
            //calcula el pixel en donde va a empezar a dibujar
            //considerando que es 320x480
            primerpix=(((480-jpeg.height)>>1)*320)+((320-(jpeg.width>>1))>>1);
            //dibuja la imagen
            for(i=0;i<jpeg.height;i++)
                {
                for(j=0;j<(jpeg.width>>1);j++)
                    xfb[whichfb][primerpix++]=jpegout[pix++]; //columnas
                primerpix=primerpix+(320-(jpeg.width>>1)); //siguiente línea
                }
            free(jpeg.outbuffer);
            //Selecciona el buffer de la imagen y trázalo
            VIDEO_SetNextFramebuffer(xfb[whichfb]);
            VIDEO_Flush();
            VIDEO_WaitVSync();
            }

        //si la imagen es mayor a 640x480
        else
            {
            width=jpeg.width;
            height=jpeg.height;
            r_aspecto=(float)width/height;
            //calcula que es mayor que 640x480... ¿el ancho o el alto?
            difancho=(float)(width-640)/r_aspecto;
            difalto=(float)height-480;
            if(difancho>difalto)
                escala=(float)width/640;
            else
                escala=(float)height/480;
            
            //Define el tamaño de la nueva imagen
            f_ancho=(float)width/escala;
            f_alto=(float)height/escala;
            n_ancho=(int)f_ancho;
            n_alto=(int)f_alto;
            
            //Para simplificar la reducción de la imagen, primero se 
            //convierte la imagen original del formato y1cby2cr (0xy1cby2cr) a un
            //formato más fácil de trabajar: ycbcr, teniendo un pixel por un int32
            jpeg_ycbcr=(unsigned int *)malloc(width*height*4);
            if(jpeg_ycbcr<=0) //no pudo dar ram
                {
                PrintA(380,"Not enough RAM (jpeg_ycbcr).");
                RefrescaVideo();
                sleep(3);
                goto next; //siguiente archivo
                }
            
            //el formato de la imagen será 0xY1CbCr00 ó 0xY2CbCr00
            j=0;
            for(i=0;i<(width*height/2);i++)
                {
                temp=jpegout[i];
                jpeg_ycbcr[j++]=((temp&0x000000ff)<<8)|(temp&0xffff0000);
                jpeg_ycbcr[j++]=((temp&0x000000ff)<<8)|((temp&0x0000ff00)<<16)|(temp&0x00ff0000);
                }
                
            //Liberar un poco de ram 
            free(jpeg.outbuffer);
            memset(&jpeg, 0, sizeof(JPEGIMG)); //pone a ceros para probable uso futuro
                
            //Teniendo la imagen separada en pixeles individuales
            //se empieza a despreciar pixeles
            //Ubica ram para la imagen escalada
            jpegtemp=(unsigned int *)malloc(n_ancho*n_alto*4);
            if(jpegtemp<=0) //no pudo dar ram
                {
                PrintA(380,"Not enough RAM (jpegtemp).");
                RefrescaVideo();
                sleep(3);
                goto next; //siguiente archivo
                }

            //calcula los pasos por ancho y alto
            paso_ancho=(float)width/n_ancho;
            paso_alto=(float)height/n_alto;
            i=0; //apunta en imagen escalada
            j=0; //apunta en imagen original
            k_float=0;
            k=0;
            cf=0; //conteo fila
            cc=0; //conteo columna
            //while((float)k_float<height)
            while(cf<n_alto)
                {
                //while((float)pix_float<width)
                while(cc<n_ancho)
                    {
                    jpegtemp[i++]=jpeg_ycbcr[(k*width)+j];
                    pix_float=(float)paso_ancho*cc++;
                    //pix_float=pix_float+paso_ancho;
                    j=(int)pix_float;
                    }
                j=0;
                pix_float=0;
                cc=0;
                k_float=(float)paso_alto*cf++;
                //k_float=k_float+paso_alto;
                k=(int)k_float;
                }
            //libera jpeg_ycbcr
            free(jpeg_ycbcr);
                
                
            //Ahora tiene que juntar dos pixeles en una sola variable,
            //pasar de ycbcr a y1cby2cr
            //Ubica ram para jpegnuevo
            jpegnuevo=(unsigned int *)malloc(n_ancho*n_alto*4);
            if(jpegnuevo<=0) //no pudo dar ram
                {
                PrintA(380,"Not enough RAM (jpegnuevo).");
                RefrescaVideo();
                sleep(3);
                goto next; //siguiente archivo
                }

            //Determina si la imagen escalada es impar o par en su ancho
            //0 es par, 1 es impar
            impar=n_ancho%2;
            
            i=0; //apunta a la imagen nueva
            j=0; //apunta a la imagen ycbcr
            
            //Imagen par en su ancho
            if(!impar)
                {
                for(i=0;i<((n_ancho>>1)*n_alto);i++)
                    {
                    //promedia croma azul cb
                    temp=(jpegtemp[j]>>16)&0x000000ff; //cb del primer pixel
                    y_1=jpegtemp[j]&0xff000000; //y del primer pixel
                    j++; //siguiente pixel
                    //le suma el cb del segundo pixel
                    temp=((jpegtemp[j]>>16)&0x000000ff)+temp;
                    y_2=(jpegtemp[j]>>16)&0x0000ff00; //y del segundo pixel
                    p_cb=temp>>1; //lo divide entre 2
                    p_cb=p_cb<<16; //lo pone en posición y1cby2cr
                    j--; //regresa al primer pixel

                    //promedia croma rojo cr
                    //lee el cr del primer pixel
                    temp=(jpegtemp[j++]>>8)&0x000000ff;        
                    //le suma el cr del segundo pixel
                    temp=((jpegtemp[j++]>>8)&0x000000ff)+temp;
                    p_cr=temp>>1; //lo divide entre 2

                    //arma el int32 con 2 pixeles (y1cby2cr)
                    jpegnuevo[i]=y_1 | p_cb | y_2 | p_cr;            
                    }
                }
            
            //Imagen impar en su ancho
            else
                {
                k=1; //apunta a la fila de la imagen nueva
                for(i=0;i<((n_ancho>>1)*n_alto);i++)
                    {
                    //promedia croma azul cb
                    temp=(jpegtemp[j]>>16)&0x000000ff; //cb del primer pixel
                    y_1=jpegtemp[j]&0xff000000; //y del primer pixel
                    j++; //siguiente pixel
                    //le suma el cb del segundo pixel
                    temp=((jpegtemp[j]>>16)&0x000000ff)+temp;
                    y_2=(jpegtemp[j]>>16)&0x0000ff00; //y del segundo pixel
                    p_cb=temp>>1; //lo divide entre 2
                    p_cb=p_cb<<16; //lo pone en posición y1cby2cr
                    j--; //regresa al primer pixel

                    //promedia croma rojo cr
                    //lee el cr del primer pixel
                    temp=(jpegtemp[j++]>>8)&0x000000ff;        
                    //le suma el cr del segundo pixel
                    temp=((jpegtemp[j++]>>8)&0x000000ff)+temp;
                    p_cr=temp>>1; //lo divide entre 2

                    //arma el int32 con 2 pixeles (y1cby2cr)
                    jpegnuevo[i]=y_1 | p_cb | y_2 | p_cr;
                        
                    //checa si hay que despreciar el último pixel de la línea actual
                    if(j==((k*n_ancho)-1))
                        {
                        k++; //siguiente línea
                        j++; //desprecia ese pixel
                        }
                    }
                n_ancho=n_ancho-1;
                }
            
            free(jpegtemp); //ya no se va a usar

            //calcula el pixel en donde va a empezar a dibujar
            //considerando que es 320x480    
            primerpix=(((480-n_alto)>>1)*320)+((320-(n_ancho>>1))>>1);
            //dibuja la imagen
            for(i=0;i<n_alto;i++)
                {
                for(j=0;j<(n_ancho>>1);j++)
                    {
                    xfb[whichfb][primerpix++]=jpegnuevo[pix++]; //columnas
                    }
                primerpix=primerpix+(320-(n_ancho>>1)); //siguiente línea
                }
                
            //libera ram
            free(jpegnuevo);
                
            //Selecciona el buffer de la imagen y trázalo
            VIDEO_SetNextFramebuffer(xfb[whichfb]);
            VIDEO_Flush();
            VIDEO_WaitVSync();
            }
    
        //Realiza la lectura del control para que el usuario
        //avance o retroceda la imagen
        next: if(LeeAB()) arc_act=arc_act-2; //presionó b, regresa una imagen
        if(arc_act==-2) arc_act=conteo-2;
        setfontsize (20);
        setfontcolour (255,0,0); //rojo
        if((arc_act+1)==conteo) //si ya fue el último archivo
            sprintf(temporal,"Next file: %s",lArchivo[0]);
        else
            sprintf(temporal,"Next file: %s",lArchivo[arc_act+1]);
        DrawText(50,425,temporal);
        } //for
    } //while
    
while(1);
}
