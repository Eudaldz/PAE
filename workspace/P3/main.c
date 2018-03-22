/******************************
 *
 * Practica_03_PAE Programació de Interrupcions
 * UB, 02/2017.
 *****************************/

#include <msp432p401r.h>
#include <stdio.h>
#include <stdint.h>
#include "lib_PAE2.h"> //Libreria grafica + configuracion reloj MSP432

#define SEQ_LEFT 1 //constante que concreta el valor 1 para representar que el sentido de la dirección será hacia la izquierda
#define SEQ_RIGHT 2 //constante que concreta el valor 1 para representar que el sentido de la dirección será hacia la izquierda
#define SEQ_INICIAL 0 //constante para representar que la secuencia de LEDs aún no ha sido iniciada (antes de usar joystick izq o der)
#define TICK_SEGONS 32768 // Número de veces que el timer contara pulsaciones de reloj para llegar al segundo
#define TICKS_MILI 33 // Número de veces que el timer contará las pulsaciones del reloj para llegar al milisegundo
#define augmentTempsRat 20 //constant per augmentar el temps relativament de la sequencia del port 7
#define minTemps 10 //constant per delimitar el temps minim de la sequencia del port 7
#define maxTemps 500 //constant per delimitar el temps maxim de la sequencia del port 7
#define lineaNormal  5
#define lineaAlarma 8
#define lineaMofidicar  6


/////////variables chars que nos serán útiles para imprimir posteriormennte en pantalla
char cadena[16];//Una linea entera con 15 caracteres visibles + uno oculto de terminacion de cadena (codigo ASCII 0)
char borrado[] = "               "; //una linea entera de 15 espacios en blanco
char modificarHora[] = "Clock (S1)"; 
char modificarAlarma[] = "Alarm  (S2)";
char horaString[] = "Hora";
char alarmaString[]="Alarma";
char modSegons[] = "      --";//Nos servirá para subrayar los segundos cuando estos sean los que se esten modificando
char modMinuts[] = "   --";//Nos servirá para subrayar los minutos cuando estos sean los que se esten modificando
char modHoras[] = "--";//Nos servirá para subrayar las horas cuando estas sean las que se esten modificando
////////


uint8_t linea = 1;
uint8_t estadoDelay = 0; //variable para controlar el counterDelay
uint8_t estado=0;//estado general servirá para controlar cual es el último GPIO accionado (empieza en 0)
uint8_t estado_anterior = 8; //
uint8_t estatSeq = SEQ_INICIAL;//variable para mantener el estado de la sequencia del puerto 7.
uint8_t estatModificar = 0; // variable para matener el estado de modificacion de reloj, o alarma, o no modificando: 0 estat neutre, 1 estat mdificant hora, 2 estat modificant alarma
uint8_t estadoTiempo = 0; // variable para mantener el estado de modificacion del reloj: 0 estado segundos, 1 estado minutos, 2 estado horas
uint32_t retraso = 1000;
uint32_t temps = 1000; //temps inicial de retard de transició de la seqüència dels LEDS del port 7.
uint32_t counterDelay = 0; //contador  timer para el metodo Delay
uint32_t counterR = 0; //contador timer para el reloj

//RELOJ
uint32_t hora = 0;
uint32_t minuts = 0;
uint32_t segons = 1; //Se inicia en 1 con el fin de que no colisione la alarma y el reloj al instante de activar el programa
/////

//////ALARMA
uint32_t horaAlarma = 0;
uint32_t minutsAlarma = 0;
uint32_t segonsAlarma = 0;
//////

/**************************************************************************
 * INICIALIZACIÓN DEL CONTROLADOR DE INTERRUPCIONES (NVIC).
 *
 * Sin datos de entrada
 *
 * Sin datos de salida
 *
 **************************************************************************/


void init_interrupciones(){
    // Configuracion al estilo MSP430 "clasico":
    // Enable Port 4 interrupt on the NVIC
    // segun datasheet (Tabla "6-12. NVIC Interrupts", capitulo "6.6.2 Device-Level User Interrupts", p80-81 del documento SLAS826A-Datasheet),
    // la interrupcion del puerto 4 es la User ISR numero 38.
    // Segun documento SLAU356A-Technical Reference Manual, capitulo "2.4.3 NVIC Registers"
    // hay 2 registros de habilitacion ISER0 y ISER1, cada uno para 32 interrupciones (0..31, y 32..63, resp.),
    // accesibles mediante la estructura NVIC->ISER[x], con x = 0 o x = 1.
    // Asimismo, hay 2 registros para deshabilitarlas: ICERx, y dos registros para limpiarlas: ICPRx.

    //Int. port 3 = 37 corresponde al bit 5 del segundo registro ISER1:
    NVIC->ICPR[1] |= BIT5; //Primero, me aseguro de que no quede ninguna interrupcion residual pendiente para este puerto,
    NVIC->ISER[1] |= BIT5; //y habilito las interrupciones del puerto
    //Int. port 4 = 38 corresponde al bit 6 del segundo registro ISERx:
    NVIC->ICPR[1] |= BIT6; //Primero, me aseguro de que no quede ninguna interrupcion residual pendiente para este puerto,
    NVIC->ISER[1] |= BIT6; //y habilito las interrupciones del puerto
    //Int. port 5 = 39 corresponde al bit 7 del segundo registro ISERx:
    NVIC->ICPR[1] |= BIT7; //Primero, me aseguro de que no quede ninguna interrupcion residual pendiente para este puerto,
    NVIC->ISER[1] |= BIT7; //y habilito las interrupciones del puerto
    //Int. TimerA0 = 8 corresponde al bit 8 del primer registro.
    NVIC->ICPR[0] |= BIT8; //Primero, me aseguro de que no quede ninguna interrupcion residual pendiente para este puerto,
    NVIC->ISER[0] |= BIT8; //y habilito las interrupciones del puerto



    __enable_interrupt(); //Habilitamos las interrupciones a nivel global del micro.
}

/**************************************************************************
 * INICIALIZACIÓN DE LA PANTALLA LCD.
 *
 * Sin datos de entrada
 *
 * Sin datos de salida
 *
 **************************************************************************/
void init_LCD(void)
{
    halLcdInit(); //Inicializar y configurar la pantallita
    halLcdClearScreenBkg(); //Borrar la pantalla, rellenando con el color de fondo
}

/**************************************************************************
 * BORRAR LINEA
 *
 * Datos de entrada: Linea, indica la linea a borrar
 *
 * Sin datos de salida
 *
 **************************************************************************/

void borrar(uint8_t Linea)
{
    halLcdPrintLine(borrado, Linea, NORMAL_TEXT); //escribimos una linea en blanco
}

/**************************************************************************
 * ESCRIBIR LINEA
 *
 * Datos de entrada: Linea, indica la linea del LCD donde escribir
 *                   String, la cadena de caracteres que vamos a escribir
 *
 * Sin datos de salida
 *
 **************************************************************************/

//Modificamos la función escribir añadiendo un parámetro que nos permita decidir si el texto que escribimos queremos que tenga los colores
//invertidos o no
void escribir(char String[], uint8_t Linea, uint8_t invert)
{
    if(!invert){
        halLcdPrintLine(String, Linea, NORMAL_TEXT); //Enviamos la String al LCD, sobreescribiendo la Linea indicada.
    }else{
        halLcdPrintLine(String, Linea, INVERT_TEXT);
    }

}

/**************************************************************************
 * INICIALIZACIÓN DE LOS BOTONES & LEDS DEL BOOSTERPACK MK II.
 *
 * Sin datos de entrada
 *
 * Sin datos de salida
 *
 **************************************************************************/
void init_botons(void)
{
    //Configuramos botones y leds
    //***************************

    //Leds RGB del MK II:
      P2SEL0 &= 0xAF;
      P2SEL1 &= 0xAF;
      P2DIR |= 0x50;  //Pines P2.4 (G), 2.6 (R) como salidas Led (RGB)
      P5SEL0 &= 0xBF;
      P5SEL1 &= 0xBF;
      P5DIR |= 0x40;  //Pin P5.6 (B)como salida Led (RGB)
      P2OUT &= 0xAF;  //Inicializamos Led RGB a 0 (apagados)
      P5OUT &= ~0x40; //Inicializamos Led RGB a 0 (apagados)

    //Boton S1 del MK II:
      P5SEL0 &= ~0x02;   //Pin P5.1 como I/O digital,
      P5SEL1 &= ~0x02;   //Pin P5.1 como I/O digital,
      P5DIR &= ~0x02; //Pin P5.1 como entrada
      P5IES &= ~0x02;   // con transicion L->H
      P5IE |= 0x02;     //Interrupciones activadas en P5.1,
      P5IFG = 0;    //Limpiamos todos los flags de las interrupciones del puerto 5
      //P5REN: Ya hay una resistencia de pullup en la placa MK II

    //Boton S2 del MK II:
      P3SEL0 &= ~0x20;   //Pin P3.5 como I/O digital,
      P3SEL1 &= ~0x20;   //Pin P3.5 como I/O digital,
      P3DIR &= ~0x20; //Pin P3.5 como entrada
      P3IES &= ~0x20;   // con transicion L->H
      P3IE |= 0x20;   //Interrupciones activadas en P3.5
      P3IFG = 0;  //Limpiamos todos los flags de las interrupciones del puerto 3
      //P3REN: Ya hay una resistencia de pullup en la placa MK II

    //Configuramos los GPIOs del joystick del MK II:
      P4DIR &= ~(BIT1 + BIT5 + BIT7);   //Pines P4.1, 4.5 y 4.7 como entrades,
      P4SEL0 &= ~(BIT1 + BIT5 + BIT7);  //Pines P4.1, 4.5 y 4.7 como I/O digitales,
      P4SEL1 &= ~(BIT1 + BIT5 + BIT7);
      P4REN |= BIT1 + BIT5 + BIT7;  //con resistencia activada
      P4OUT |= BIT1 + BIT5 + BIT7;  // de pull-up
      P4IE |= BIT1 + BIT5 + BIT7;   //Interrupciones activadas en P4.1, 4.5 y 4.7,
      P4IES &= ~(BIT1 + BIT5 + BIT7);   //las interrupciones se generaran con transicion L->H
      P4IFG = 0;    //Limpiamos todos los flags de las interrupciones del puerto 4

      P5DIR &= ~(BIT4 + BIT5);  //Pines P5.4 y 5.5 como entrades,
      P5SEL0 &= ~(BIT4 + BIT5); //Pines P5.4 y 5.5 como I/O digitales,
      P5SEL1 &= ~(BIT4 + BIT5);
      P5IE |= BIT4 + BIT5;  //Interrupciones activadas en 5.4 y 5.5,
      P5IES &= ~(BIT4 + BIT5);  //las interrupciones se generaran con transicion L->H
      P5IFG = 0;    //Limpiamos todos los flags de las interrupciones del puerto 4
    // - Ya hay una resistencia de pullup en la placa MK II
}

void init_timer(void){
    //Configuración del Timer A0
    TA0CTL = BIT8 + BIT4 + BIT1;
    TA0CCTL0 = CCIE;
    TA0CCR0 = TICKS_MILI; //configuramos el timer para que llame a la interrupcion una vez  por milisegundo.
}

/**************************************************************************
 * DELAY - A CONFIGURAR POR EL ALUMNO
 *
 * 
 *
 * Sin datos de salida
 *
 **************************************************************************/
void delay_t (uint32_t temps)
{
   counterDelay = 0;
   estadoDelay = 1; //activamos el contador en la interrupcion del timer
   while(counterDelay < temps); //couterDelay se va incrementando en la rutina de interrupcion del contador.
   estadoDelay = 0; //desactivamos el contador en la interrupcion: ya no es necessario
}

//metodo para augmentar la variable de tiempo de la sequencia
void augmentarTemps(void){
    temps+=temps*augmentTempsRat / 100;
    if(temps > maxTemps)temps = maxTemps;
}

//metodo para disminuir la variable de tiempo de la sequencia
void disminuirTemps(){
    temps -= temps*augmentTempsRat / 100;
    if(temps < minTemps)temps = minTemps;
}

/*****************************************************************************
 * CONFIGURACIÓN DEL PUERTO 7. A REALIZAR POR EL ALUMNO
 *
 * Sin datos de entrada
 *
 * Sin datos de salida
 *
 ****************************************************************************/
void config_P7_LEDS (void)
{
    P7SEL0 = 0x00;
    P7SEL1 = 0x00; //Tots els leds I/O digital
    P7DIR = 0xFF; //Tots son sortida
    P7OUT = 0x00; //Init apagats
}


void main(void)
{

    WDTCTL = WDTPW+WDTHOLD;         // Paramos el watchdog timer

    //Inicializaciones:
    init_ucs_24MHz(); //Ajustes del clock (Unified Clock System)
    init_timer();
    init_botons();         //Configuramos botones y leds
    init_interrupciones();  //Configurar y activar las interrupciones de los botones
    init_LCD(); // Inicializamos la pantalla
    config_P7_LEDS(); //Inicialitzem els leds PORT 7.

    escribir(modificarHora,0, 0);
    escribir(modificarAlarma,1, 0);
    linea++;
    escribir(horaString, 4, 0);
    escribir(alarmaString, 7, 0);
    printHora(lineaNormal);
    printAlarma(lineaAlarma);
    //Bucle principal (infinito):
    do
    {

        if (estado_anterior != estado)          // Dependiendo del valor del estado se encenderá un LED u otro.
        {

            estado_anterior = estado;          // Actualizamos el valor de estado_anterior, para que no esté siempre escribiendo.

            /**********************************************************+
                A RELLENAR POR EL ALUMNO BLOQUE switch ... case
            Para gestionar las acciones:
            Boton S1, estado = 1
            Boton S2, estado = 2
            Joystick left, estado = 3
            Joystick right, estado = 4
            Joystick up, estado = 5
            Joystick down, estado = 6
            Joystick center, estado = 7
            ***********************************************************/
            switch(estado){
            case 0: //caso inicial
                 P2OUT ^= 0x40;      // Conmutamos el estado del LED R (bit 6)
                 delay_t(retraso);  // periodo del parpadeo
                 P2OUT ^= 0x10;     // Conmutamos el estado del LED G (bit 4)
                 delay_t(retraso);  // periodo del parpadeo
                 P5OUT ^= 0x40;     // Conmutamos el estado del LED B (bit 6)
                 delay_t(retraso);  // periodo del parpadeo
                 break;
            case 1: //boto s1
                P2OUT |= 0x50; //activem LED R i LED G
                P5OUT |= 0x40; //activem LED B
                break;
            case 2: //boto s2
                P2OUT &= 0xAF; //desactivem LED R i LED G
                P5OUT &= 0xBF; //desactivem LED B
                break;
            case 3://joystick left
                P2OUT |= 0x50; //activem LED R i LED G
                P5OUT |= 0x40; //activem LED B
                estatSeq = SEQ_LEFT;
                break;
            case 4://joystick right
                P2OUT |= 0x50; //activem LED R i LED G
                P5OUT &= 0xBF; //desactivem LED B
                estatSeq = SEQ_RIGHT;
                break;
            case 5://joystick up
                P5OUT |= 0x40; //activem LED B
                P2OUT |= 0x40; //activem LED R
                P2OUT &= 0xE0; //desactivem LED G
                break;
            case 6://joystick down
                P5OUT |= 0x40; //activem LED B
                P2OUT |= 0x10; //activem LED G
                P2OUT &= 0xBF; //desactivem LED R
                break;
            case 7://joystick center
                P5OUT ^= 0x40; //invertim LED B
                P2OUT ^= 0x50; //invertim LEDS R i G
                break;
            }
        }

        if(estatSeq == SEQ_RIGHT ){
            seq_dreta(temps); //cridem funció que executa la sequencia de LEDS cap a la dreta.
        }
        else if(estatSeq == SEQ_LEFT ){
            seq_esquerra(temps);//cridem funció que executa la sequencia de LEDS cap a lesquerra.
        }

    }while(1); //Condicion para que el bucle sea infinito
}

//metodo para imprimir la hora en patanlla
//parametro: linea en pantalla

void printHora(uint8_t linea2){
    char aux[9];
    sprintf(aux,"%2d:%2d:%2d",hora, minuts,segons);
    escribir(aux,linea2,0);
}

//metodo para imprimir la hora en pantalla
//parametro: linea en pantalla
void printAlarma(uint8_t linea2){
    char aux[9];
    sprintf(aux,"%2d:%2d:%2d",horaAlarma, minutsAlarma, segonsAlarma);
    escribir(aux,linea2,0);
}

//metodo que incrementa en una unidad, la unidad del reloj <segundos, minutos, horas> segun la variable estadoTiempo y estatModificar
//este metodo utiliza submetodos separados para reutilizar el incremento de segundos para el contador timer del reloj.
void incrementar(void){

    switch(estadoTiempo){
        case 0:
        incrementar_segons(estatModificar);
        break;
        case 1:
        incrementar_minuts(estatModificar);
        break;
        case 2:
        incrementar_hora(estatModificar);
        break;
    }

    printHora(lineaNormal);
    printAlarma(lineaAlarma);
}

//metodo para decrementar en una unidad, la unidad del reloj <segundos, minutos, horas> segun la variable estadoTiempo.
void decrementar(void){
    switch(estadoTiempo){
    case 0:
        if(estatModificar ==1){
            if(segons >= 1){
                    segons -= 1;
                }
        }if(estatModificar == 2){
            if(segonsAlarma >= 1){
                segonsAlarma -= 1;
            }
        }
        break;
    case 1:
        if(estatModificar ==1){
               if(minuts >= 1){
                       minuts -= 1;
                   }
           }if(estatModificar == 2){
               if(minutsAlarma >= 1){
                   minutsAlarma -= 1;
               }
          }
    break;

    case 2:
        if(estatModificar ==1){
           if(hora >= 1){
               hora -= 1;
           }
       }if(estatModificar == 2){
           if(horaAlarma >= 1){
               horaAlarma -= 1;
           }
      }
       break;

    }
    printHora(lineaNormal);
    printAlarma(lineaAlarma);
}

/*metodo que incrementa segundos del reloj o la alarma segun el parametro
parametro: indica si se tiene que incrementar los segundos del reloj o de la alarma.
se podia hacer con dos metodos separados y sin parametros.
*/
void incrementar_segons(uint8_t estatModificar){
    if(estatModificar == 1){
        if(segons == 59){
            incrementar_minuts(estatModificar);
            segons = 0;
        }else{
            segons+=1;
        }
    }else if (estatModificar == 2){
        if(segonsAlarma == 59){
           incrementar_minuts(estatModificar);
           segonsAlarma = 0;
        }else{
           segonsAlarma+=1;
        }
    }
}

//metodo que incrementa minutos del reloj o la alarma segun el parametro.
void incrementar_minuts(uint8_t estatModificar){
    if(estatModificar == 1){
        if(minuts == 59){
            incrementar_hora(estatModificar);
            minuts = 0;
        }else{
            minuts += 1;
        }
    }else if(estatModificar == 2){
        if(minutsAlarma == 59){
            incrementar_hora(estatModificar);
            minutsAlarma = 0;
        }else{
            minutsAlarma += 1;
        }
    }
}

//metodo que incrementa minutos del reloj o la alarma segun el parametro.
void incrementar_hora(uint8_t estatModificar){
    if(estatModificar == 1){
        hora = (hora + 1)%24;
    }else if(estatModificar == 2){
        horaAlarma = (horaAlarma + 1)%24;
    }
}
/**
 * SEQUENCIA DE LEDS-PORT 7 CAP A L'ESQUERRA
 * Amb aquesta rutina s'activen els LEDS del port 7 successivament un per un des de l'ultim fins al primer (cap a l'esquerra des del punt de vista de l'usuari)
 *
 * Dades d'entrada: temps de delay entre activació de leds consecutius.
 *
 * Sense valor de sortida
 *
 **/
void seq_esquerra(void){
    for(P7OUT = 128; P7OUT > 0; P7OUT >>= 1){
        delay_t(temps);
    }
    //delay_t(temps);
    P7OUT = 0;
}

/**
 * SEQUENCIA DE LEDS-PORT 7 CAP A LA DRETA
 * Amb aquesta rutina s'activen els LEDS del port 7 successivament un per un des del primer fins a l'ultim (cap a la dreta des del punt de vista de l'usuari)
 *
 * Dades d'entrada: temps de delay entre activació de leds consecutius.
 *
 * Sense valor de sortida
 *
 **/
void seq_dreta(void){
    for(P7OUT = 1; P7OUT < 128; P7OUT <<= 1){
        delay_t(temps);
    }
    delay_t(temps);
    P7OUT = 0;
}


/**************************************************************************
 * RUTINAS DE GESTION DE LOS BOTONES:
 * Mediante estas rutinas, se detectará qué botón se ha pulsado
 *
 * Sin Datos de entrada
 *
 * Sin datos de salida
 *
 * Actualizar el valor de la variable global estado
 *
 **************************************************************************/

//ISR para las interrupciones del puerto 3:
void PORT3_IRQHandler(void){//interrupcion del pulsador S2
    uint8_t flag = P3IV; //guardamos el vector de interrupciones. De paso, al acceder a este vector, se limpia automaticamente.
    P3IE &= 0xDF;  //interrupciones del boton S2 en port 3 desactivadas
    estado_anterior=0;

    /**********************************************************+
        A RELLENAR POR EL ALUMNO
    Para gestionar los estados:
    Boton S1(5.1), estado = 1
    Boton S2(3.5), estado = 2
    Joystick left(4.7), estado = 3
    Joystick right(4.5), estado = 4
    Joystick up(5.4), estado = 5
    Joystick down(5.5), estado = 6
    Joystick center(4.1), estado = 7

    FLAG INTERUPCIONES:
    0x02: p.0
    0x04: p.1
    0x06: p.2
    0x08: p.3
    0x0A: p.4
    0x0C: p.5
    0x0E: p.6
    0x10: p.7
    ***********************************************************/
    switch(flag){
    case 0x0C: //pin 3.5
        estado = 2;
        disminuirTemps();
        estatModificar = 2;
        escribir(modificarAlarma, 1, 1);
        escribir(modificarHora, 0, 0);
        printAuxMod();
        break;
    }

    P3IE |= 0x20;   //interrupciones S2 en port 3 reactivadas
}

//ISR para las interrupciones del puerto 4:
void PORT4_IRQHandler(void){  //interrupción de los botones. Actualiza el valor de la variable global estado.
    uint8_t flag = P4IV; //guardamos el vector de interrupciones. De paso, al acceder a este vector, se limpia automaticamente.
    P4IE &= 0x5D;   //interrupciones Joystick en port 4 desactivadas
    estado_anterior=0;

    /**********************************************************+
        A RELLENAR POR EL ALUMNO BLOQUE switch ... case
    Para gestionar los estados:
    Boton S1 (5.1), estado = 1
    Boton S2(3.5), estado = 2
    Joystick left(4.7), estado = 3
    Joystick right(4.5, estado = 4
    Joystick up(5.4), estado = 5
    Joystick down(5.5), estado = 6
    Joystick center(4.1), estado = 7

    FLAG INTERUPCIONES:
    0x02: p.0
    0x04: p.1
    0x06: p.2
    0x08: p.3
    0x0A: p.4
    0x0C: p.5
    0x0E: p.6
    0x10: p.7
    ***********************************************************/
    switch(flag){
    case 0x04: //4.1
        estado = 7;
        estatModificar = 0;
        escribir(modificarHora, 0, 0);
        escribir(modificarAlarma, 1, 0);
        borrar(3);
        printAuxMod();
        break;
    case 0x0C: //4.5
        estado = 4;
        estadoTiempo = (estadoTiempo - 1) % 3;
        printAuxMod();
        break;
    case 0x10: //4.7
        estado = 3;
        estadoTiempo = (estadoTiempo + 1) % 3;
        printAuxMod();
        break;
    }


    /***********************************************
     * HASTA AQUI BLOQUE CASE
     ***********************************************/

    P4IE |= 0xA2;   //interrupciones Joystick en port 4 reactivadas
}

void printAuxMod(){
    borrar(6);
    borrar(9);
    if(estatModificar == 1){
        if(estadoTiempo == 0)escribir(modSegons, 6 ,0);
        if(estadoTiempo == 1)escribir(modMinuts, 6, 0);
        if(estadoTiempo == 2)escribir(modHoras, 6, 0);
    }else if(estatModificar == 2){
        if(estadoTiempo == 0)escribir(modSegons, 9 ,0);
        if(estadoTiempo == 1)escribir(modMinuts, 9, 0);
        if(estadoTiempo == 2)escribir(modHoras, 9, 0);
    }
}

//ISR para las interrupciones del puerto 5:
void PORT5_IRQHandler(void){  //interrupción de los botones. Actualiza el valor de la variable global estado.
    uint8_t flag = P5IV; //guardamos el vector de interrupciones. De paso, al acceder a este vector, se limpia automaticamente.
    P5IE &= 0xCD;   //interrupciones Joystick y S1 en port 5 desactivadas
    estado_anterior=0;

    /**********************************************************+
        A RELLENAR POR EL ALUMNO BLOQUE switch ... case
    Para gestionar los estados:
    Boton S1(5.1), estado = 1
    Boton S2(3.5), estado = 2
    Joystick left(4.7), estado = 3
    Joystick right(4.5, estado = 4
    Joystick up(5.4), estado = 5
    Joystick down(5.5), estado = 6
    Joystick center(4.1), estado = 7

    FLAG INTERUPCIONES:
    0x02: p.0
    0x04: p.1
    0x06: p.2
    0x08: p.3
    0x0A: p.4
    0x0C: p.5
    0x0E: p.6
    0x10: p.7
    ***********************************************************/
    switch(flag){
    case 0x04: //5.1
        estado = 1;
        augmentarTemps();
        estatModificar = 1;
        escribir(modificarHora, 0, 1);
        escribir(modificarAlarma, 1, 0);
        printAuxMod();
        break;
    case 0x0A: //5.4
        estado = 5;
        augmentarTemps();
        incrementar();
        break;
    case 0x0C: //5.5
        disminuirTemps();
        estado = 6;
        decrementar();
        break;
    }

    /***********************************************
     * HASTA AQUI BLOQUE CASE
     ***********************************************/

    P5IE |= 0x32;   //interrupciones Joystick y S1 en port 5 reactivadas
}

void TA0_0_IRQHandler(void){
    TA0CCTL0 &= ~CCIE;
    //TA0R = 0;
    if(estadoDelay == 1)counterDelay += 1; //de esta forma solo contamos cuando sea necesario. estadoDelay se activa en el metodo delay(tiempo)
    
    counterR += 1; //este es el counter del reloj, se resetea cada vez que llega a 1000 (un segundo)

    if (counterR == 1000){ //ha pasado un segundo, incrementa el reloj i resetea el counter.
        if(estatModificar != 1){
            incrementar_segons(1);
            printHora(lineaNormal); //actualiza la pantalla para mostrar la hora correcta.
        }

        if(estatModificar == 0 && hora == horaAlarma && minuts == minutsAlarma && segons == segonsAlarma){
            escribir("Congratulations!", 3, 1);
        }
        counterR = 0;
    }

    TA0CCTL0 &= ~CCIFG;
    TA0CCTL0 |= CCIE;

}
