/******************************
 *
 * Practica_03_PAE Programaci� de Interrupcions
 * UB, 02/2017.
 *****************************/

#include <msp432p401r.h>
#include <stdio.h>
#include <stdint.h>
#include "lib_PAE2.h"> //Libreria grafica + configuracion reloj MSP432

#define SEQ_LEFT 1
#define SEQ_RIGHT 2
#define SEQ_NULL 0
#define TICK_SEGONS 32768
#define TICKS_MILI 33

char saludo[16] = " HOLA";//max 15 caracteres visibles
char cadena[16];//Una linea entera con 15 caracteres visibles + uno oculto de terminacion de cadena (codigo ASCII 0)
char borrado[] = "               "; //una linea entera de 15 espacios en blanco
uint8_t linea = 1;
uint8_t estado=0;
uint8_t estado_anterior = 8;
uint8_t estatSeq = SEQ_NULL;
uint8_t estatSeqAnterior = 0;
uint32_t retraso = 1000;
uint32_t temps = 1000; //temps inicial de retard de transici� de la seq��ncia dels LEDS del port 7.
uint32_t augmentTemps = 100; //constant per augmentar el temps de transici� dels leds.
uint32_t counter = 0;

/**************************************************************************
 * INICIALIZACI�N DEL CONTROLADOR DE INTERRUPCIONES (NVIC).
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
 * INICIALIZACI�N DE LA PANTALLA LCD.
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
void escribir(char String[], uint8_t Linea)

{
    halLcdPrintLine(String, Linea, NORMAL_TEXT); //Enviamos la String al LCD, sobreescribiendo la Linea indicada.
}

/**************************************************************************
 * INICIALIZACI�N DE LOS BOTONES & LEDS DEL BOOSTERPACK MK II.
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
    //Configuraci�n del Timer A0

    TA0CTL = BIT8 + BIT4 + BIT1;
    TA0CCTL0 = CCIE;
    TA0CCR0 = TICKS_MILI;
}

/**************************************************************************
 * DELAY - A CONFIGURAR POR EL ALUMNO - con bucle while
 *
 * Datos de entrada: Tiempo de retraso. 1 segundo equivale a un retraso de 1000000 (aprox)
 *
 * Sin datos de salida
 *
 **************************************************************************/
void delay_t (uint32_t temps)
{
   counter = 0;
   while(counter < temps); //bucle tonto para perder tiempo.
}

void augmentarTemps(void){
    temps+=augmentTemps;
}

void disminuirTemps(){
    temps = temps > 0 ? temps-augmentTemps : 100;
}

/*****************************************************************************
 * CONFIGURACI�N DEL PUERTO 7. A REALIZAR POR EL ALUMNO
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

/*void main(void){
    WDTCTL = WDTPW+WDTHOLD;
    init_ucs_16MHz(); //Ajustes del clock (Unified Clock System)
    init_timer();
    init_botons();         //Configuramos botones y leds
    init_interrupciones();  //Configurar y activar las interrupciones de los botones
    init_LCD(); // Inicializamos la pantalla
    config_P7_LEDS(); //Inicialitzem els leds PORT 7.
    while(1);
}*/

void main(void)
{

    WDTCTL = WDTPW+WDTHOLD;         // Paramos el watchdog timer

    //Inicializaciones:
    init_ucs_16MHz(); //Ajustes del clock (Unified Clock System)
    init_timer();
    init_botons();         //Configuramos botones y leds
    init_interrupciones();  //Configurar y activar las interrupciones de los botones
    init_LCD(); // Inicializamos la pantalla
    config_P7_LEDS(); //Inicialitzem els leds PORT 7.

    halLcdPrintLine(saludo,linea, INVERT_TEXT); //escribimos saludo en la primera linea
    linea++;                    //Aumentamos el valor de linea y con ello pasamos a la linea siguiente

    //Bucle principal (infinito):
    do
    {
        if (estado_anterior != estado)          // Dependiendo del valor del estado se encender� un LED u otro.
        {
            sprintf(cadena," estado %d", estado);    // Guardamos en cadena la siguiente frase: estado "valor del estado"
            escribir(cadena,linea);          // Escribimos la cadena al LCD
            estado_anterior = estado;          // Actualizamos el valor de estado_anterior, para que no est� siempre escribiendo.

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
                //if(estatSeq == SEQ_NULL){estatSeq = estatSeqAnterior;}
                break;
            case 2: //boto s2
                P2OUT &= 0xAF; //desactivem LED R i LED G
                P5OUT &= 0xBF; //desactivem LED B
                //estatSeqAnterior = estatSeq;
                //estatSeq = SEQ_NULL;
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
            seq_dreta(temps); //cridem funci� que executa la sequencia de LEDS cap a la dreta.
            //sprintf(cadena," seq dreta", estado);
        }
        else if(estatSeq == SEQ_LEFT ){
            seq_esquerra(temps);//cridem funci� que executa la sequencia de LEDS cap a lesquerra.
            //sprintf(cadena," seq esquerra", estado);
        }

        //delay_t(5000);
    }while(1); //Condicion para que el bucle sea infinito
}

/**
 * SEQUENCIA DE LEDS-PORT 7 CAP A L'ESQUERRA
 * Amb aquesta rutina s'activen els LEDS del port 7 successivament un per un des de l'ultim fins al primer (cap a l'esquerra des del punt de vista de l'usuari)
 *
 * Dades d'entrada: temps de delay entre activaci� de leds consecutius.
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
 * Dades d'entrada: temps de delay entre activaci� de leds consecutius.
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
 * Mediante estas rutinas, se detectar� qu� bot�n se ha pulsado
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
        break;
    }

    P3IE |= 0x20;   //interrupciones S2 en port 3 reactivadas
}

//ISR para las interrupciones del puerto 4:
void PORT4_IRQHandler(void){  //interrupci�n de los botones. Actualiza el valor de la variable global estado.
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
        break;
    case 0x0C: //4.5
        estado = 4;
        break;
    case 0x10: //4.7
        estado = 3;
        break;
    }


    /***********************************************
     * HASTA AQUI BLOQUE CASE
     ***********************************************/

    P4IE |= 0xA2;   //interrupciones Joystick en port 4 reactivadas
}

//ISR para las interrupciones del puerto 5:
void PORT5_IRQHandler(void){  //interrupci�n de los botones. Actualiza el valor de la variable global estado.
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
        break;
    case 0x0A: //5.4
        estado = 5;
        augmentarTemps();
        break;
    case 0x0C: //5.5
        disminuirTemps();
        estado = 6;
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
    counter += 1;
    //P2OUT ^= 0x40;      // Conmutamos el estado del LED R (bit 6)
    //P2OUT ^= 0x10;     // Conmutamos el estado del LED G (bit 4)
    //P5OUT ^= 0x40;     // Conmutamos el estado del LED B (bit 6)
    TA0CCTL0 &= ~CCIFG;
    TA0CCTL0 |= CCIE;

}
