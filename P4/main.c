#include <msp432p401r.h>
#include <stdio.h>
#include <stdint.h>
#include "lib_PAE2.h" //Libreria grafica + configuracion reloj MSP432

typedef uint8_t byte;
#define TXD2_READY (UCA2IFG & UCTXIFG)

//==DIRECTION================
//===========================

#define FORWARD_RIGHT 4
#define BACKWARD_RIGHT 0
#define FORWARD_LEFT 4
#define BACKWARD_LEFT 0

//===========================
//===========================



//==SERVO_ID=================
//===========================

#define SERVO_RIGHT 2
#define SERVO_LEFT 3


//===========================
//===========================



//==INSTRUCTIONS=============
//===========================

#define PING 1
#define READ 2
#define WRITE 3
#define REG_WRITE 4
#define ACTION 5
#define RESET 6

//==========================
//==========================



//==SPEEDS==================
//==========================

#define LOW_SPEED 100
#define NORMAL_SPEED 300
#define HIGH_SPEED 500

//==========================
//==========================



#define OVERWRITE_TEXT 1
char saludo[16] = " PRACTICA 4 PAE";//max 15 caracteres visibles
char borrado[] = "               "; //una linea entera de 15 espacios en blanco

uint8_t linea = 1;
uint8_t timer = 0;  // Contador del timer A0
uint8_t Byte_Recibido = 0;
byte parametros[16];
uint16_t velocidad_right = 300;
uint16_t velocidad_left = 300;
uint8_t readIndex = 0;
byte toggleRead = 0;
byte readingProcessState = 0; //Indicates if error or success



typedef struct DataPacket{
    byte id;
    byte parameterLength;
    byte instruction;
    byte parametros[32];
}DataPacket;

DataPacket readPacket;
/**************************************************************************
 * INICIALIZACIÓN DEL CONTROLADOR DE INTERRUPCIONES (NVIC).
 *
 * Sin datos de entrada
 *
 * Sin datos de salida
 *
 **************************************************************************/
void init_interrupciones(){

    //timer A0
    NVIC->ICPR[0] |= BIT8;
    NVIC->ISER[0] |= BIT8;
    //UART
    NVIC->ICPR[0] |= 1<<18;
    NVIC->ISER[0] |= 1<<18;
    __enable_interrupt(); //Habilitamos las interrupciones a nivel global del micro.
}


void initializeReadPacket(){
    readPacket.id = 0;
    readPacket.parameterLength = 0;
    readPacket.instruction = 0;
    byte i;
    for(i = 0; i<32; i++){
        readPacket.parametros[i] = 0;
    }
}

DataPacket newDataPacket(){
    DataPacket new;
    new.id = 0;
    new.parameterLength = 0;
    new.instruction = 0;
    byte i;
    for(i = 0; i<32; i++){
        new.parametros[i] = 0;
    }
    return new;
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
    halLcdPrintLine(borrado, Linea, OVERWRITE_TEXT); //escribimos una linea en blanco
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
    halLcdPrintLine(String, Linea, OVERWRITE_TEXT); //Enviamos la String al LCD, sobreescribiendo la Linea indicada.
}

/**************************************************************************
 * INICIALIZACIÓN DE LOS TIMERS A0 Y A1.
 **************************************************************************/
void init_timers(void){
    //A0
    TA0CTL &= ~(BIT9 + BIT8); //Nos aseguramos que esten los bits a cero inicialmente para evitar posibles complicaciones
    TA0CTL &= ~(BIT4 + BIT5); //Nos aseguramos que esten los bits a cero inicialmente para evitar posibles complicaciones
    TA0CCTL0 &= ~CCIFG; // Interrupt pending
    TA0CTL |= TASSEL__SMCLK+MC__STOP; // Timer parado (No queremos que comience a contar desde un inicio)
    TA0CCTL0 |= CCIE; // Interrupt enabled
    TA0CCTL0 &= ~CAP; // Mode Compare
    TA0CCR0 = 24000; // Data para la comparacion del timer

}

void init_UART(void) {
    UCA2CTLW0 |= UCSWRST;       //Fem un reset de la USCI, desactiva la USCI
    UCA2CTLW0 |= UCSSEL__SMCLK; //UCSYNC=0 mode asíncron
                                //UCMODEx=0 seleccionem mode UART
                                //UCSPB=0 nomes 1 stop bit
                                //UC7BIT=0 8 bits de dades
                                //UCMSB=0 bit de menys pes primer
                                //UCPAR=x ja que no es fa servir bit de paritat
                                //UCPEN=0 sense bit de paritat
                                //Triem SMCLK (24MHz) com a font del clock BRCLK
    UCA2MCTLW = UCOS16; // Necessitem sobre-mostreig => bit 0 = UCOS16 = 1
    UCA2BRW = 3;    //Prescaler de BRCLK fixat a 3. Com SMCLK va a24MHz,
                    //volem un baud rate de 500kb/s i fem sobre-mostreig de 16
                    //el rellotge de la UART ha de ser de 8MHz (24MHz/3).
                    //Configurem els pins de la UART
    P3SEL0 |= BIT2 | BIT3; //I/O funció: P3.3 = UART2TX, P3.2 = UART2RX
    P3SEL1 &= ~ (BIT2 | BIT3);
            //Configurem pin de selecció del sentit de les dades Transmissió/Recepeció
    P3SEL0 &= ~BIT0; //Port P3.0 com GPIO
    P3SEL1 &= ~BIT0;
    P3DIR |= BIT0; //Port P3.0 com sortida (Data Direction selector Tx/Rx)
    P3OUT &= ~BIT0; //Inicialitzem Sentit Dades a 0 (Rx)
    UCA2CTLW0 &= ~UCSWRST; //Reactivem la línia de comunicacions sèrie
    UCA2IE |= UCRXIE; //Això només s’ha d’activar quan tinguem la rutina de recepció
}

//Defines

/* funcions per canviar el sentit de les comunicacions */
void Sentit_Dades_Rx(void) { //Configuració del Half Duplex dels motors: Recepció
    P3OUT &= ~BIT0; //El pin P3.0 (DIRECTION_PORT) el posem a 0 (Rx)
}
void Sentit_Dades_Tx(void) { //Configuració del Half Duplex dels motors: Transmissió
    P3OUT |= BIT0; //El pin P3.0 (DIRECTION_PORT) el posem a 1 (Tx)
}


void setInterruptionActive(byte isActive){
    if(!isActive){
        UCA2IE &= ~UCRXIE;
    }
    UCA2IE |= UCRXIE;
}

DataPacket RxPacket(){
    initializeReadPacket();
    readIndex = 0;
    setInterruptionActive(1);
    readingProcessState = 0;
    DataPacket empty = newDataPacket();
    Sentit_Dades_Rx();
    while(readingProcessState == 0);
    toggleRead = 0;
    if(readingProcessState == 2){
        setInterruptionActive(0);
        return readPacket;
    }
    escribir("Fallo", 6);
    setInterruptionActive(0);
    return empty;
}

/* funció TxUAC0: envia un array de bytes a la UART  */
void TxUAC2(byte* bTxdData, byte length) {
    Sentit_Dades_Tx();
    byte i;
    for(i = 0; i < length; i++){
        while(!TXD2_READY); // Espera a que estigui preparat el buffer de transmissió
        UCA2TXBUF = bTxdData[i];
    }
    while( (UCA2STATW&UCBUSY));
    Sentit_Dades_Rx();
}

void TxUAC2_2(byte data){
    while(!TXD2_READY);
    UCA2TXBUF = data;
}

/*void TxDByte(byte bData){
    while(!TXD_BUFFER_READY_BIT); //wait until data can be loaded.
    SerialTxDBuffer = bData; //data load to TxD buffer
}*/

//TxPacket() 3 paràmetres: ID del Dynamixel, Mida dels paràmetres, Instruction byte. torna la mida del "Return packet"
byte TxPacket(DataPacket dp) {
    if(dp.instruction == 0x03 && dp.parametros[0] <= 0x05)return 0;
    byte bCount,bCheckSum,bPacketLength;
    byte TxBuffer[32];
    TxBuffer[0] = 0xff; //Primers 2 bytes que indiquen inici de trama FF, FF.
    TxBuffer[1] = 0xff;
    TxBuffer[2] = dp.id; //ID del mòdul al que volem enviar el missatge
    TxBuffer[3] = dp.parameterLength+2; //Length(Parameter,Instruction,Checksum)
    TxBuffer[4] = dp.instruction; //Instrucció que enviem al Mòdul

    for(bCount = 0; bCount < dp.parameterLength; bCount++) { //Comencem a generar la trama que hem d’enviar
        TxBuffer[bCount+5] = dp.parametros[bCount];
    }

    bCheckSum = 0;
    bPacketLength = dp.parameterLength+4+2;
    for(bCount = 2; bCount < bPacketLength-1; bCount++) { //Càlcul del checksum
        bCheckSum += TxBuffer[bCount];
    }

    TxBuffer[bCount] = ~bCheckSum; //Escriu el Checksum (complement a 1)

    TxUAC2(TxBuffer, bCount+1);
    return(bPacketLength);
}


//TxPacket() 3 paràmetres: ID del Dynamixel, Mida dels paràmetres, Instruction byte. torna la mida del "Return packet"
byte TxPacket2(byte bID, byte bParameterLength, byte bInstruction, byte Parametros[32]) {
    byte bCount,bCheckSum,bPacketLength;
    byte TxBuffer[32];
    Sentit_Dades_Tx(); //El pin P3.0 (DIRECTION_PORT) el posem a 1 (Transmetre)
    TxBuffer[0] = 0xff; //Primers 2 bytes que indiquen inici de trama FF, FF.
    TxBuffer[1] = 0xff;
    TxBuffer[2] = bID; //ID del mòdul al que volem enviar el missatge
    TxBuffer[3] = bParameterLength+2; //Length(Parameter,Instruction,Checksum)
    TxBuffer[4] = bInstruction; //Instrucció que enviem al Mòdul
    for(bCount = 0; bCount < bParameterLength; bCount++) { //Comencem a generar la trama que hem d’enviar
        TxBuffer[bCount+5] = Parametros[bCount];
    }

    bCheckSum = 0;
    bPacketLength = bParameterLength+4+2;
    for(bCount = 2; bCount < bPacketLength-1; bCount++) { //Càlcul del checksum
        bCheckSum += TxBuffer[bCount];
    }

    TxBuffer[bCount] = ~bCheckSum; //Escriu el Checksum (complement a 1)
    for(bCount = 0; bCount < bPacketLength; bCount++) { //Aquest bucle és el que envia la trama al Mòdul Robot
        TxUAC2_2(TxBuffer[bCount]);
    }

    while( (UCA2STATW&UCBUSY)); //Espera fins que s’ha transmès el últim byte
    Sentit_Dades_Rx(); //Posem la línia de dades en Rx perquè el mòdul Dynamixel envia resposta
    return(bPacketLength);
}

/**************************************************************************
 * DELAY - con timer A0 ( Nota: Esta función es solo para el apartado 1 de la practica )
 **************************************************************************/
void delay_t (uint32_t temps)
{

    TA0CTL |= MC__UP; // Timer encendido (Ahora si comienza a contar el timer)
    while(timer < temps){} // Bucle necesario para esperar a que el timer llegue al tiempo deseado
    TA0CTL &= ~MC__UP; // Timer parado
    timer = 0; //Reiniciamos el contador


}

/*void CW_Angle_Limit(byte id){
    parametros[0] = 6;
    parametros[1] = 0;
    parametros[2] = 0;
    parametros[3] = 0;
    parametros[4] = 0;
    TxPacket(id,5,3,parametros);
    delay_t(100);
}*/

DataPacket getPacket_MoveServo(byte id, uint16_t speed, byte sentido){
    DataPacket dp;
    dp.id = id;
    dp.parameterLength = 3;
    dp.instruction = 0x03;
    dp.parametros[0] = 0x20;
    dp.parametros[1] = (byte)speed;
    dp.parametros[2] = (byte)(speed>>8) + sentido;
    return dp;
}

DataPacket getPacket_Ping(byte id){
    DataPacket dp;
    dp.id = id;
    dp.parameterLength = 0x0;
    dp.instruction = 0x01;
    return dp;
}

DataPacket getPacket_Led(byte id, byte setActive){
    DataPacket dp;
    dp.id = id;
    dp.parameterLength = 2;
    dp.instruction = 0x03;
    dp.parametros[0] = 0x19;
    dp.parametros[1] = 0x01;
    return dp;
}

/**
 * main.c
 */
void printPacketOnLCD(DataPacket dp){
    char s[1];
    sprintf(s, "%02x", dp.instruction);
    escribir(s, 7);
}



void main(void)
  {
	WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;		// stop watchdog timer

    //Inicializaciones:
    init_ucs_24MHz();       //Ajustes del clock (Unified Clock System)
    init_interrupciones();
    init_timers();
    init_LCD();
    init_UART();
    setInterruptionActive(0);
    halLcdPrintLine(saludo,linea, INVERT_TEXT); //escribimos saludo en la primera linea
    linea++;                    //Aumentamos el valor de linea y con ello pasamos a la linea siguiente

    DataPacket servo_left = getPacket_Ping(SERVO_LEFT);
    //DataPacket servo_right = getPacket_MoveServo(SERVO_RIGHT, NORMAL_SPEED, FORWARD_RIGHT);
    TxPacket(servo_left);
    DataPacket servo_left_status = RxPacket();
    //TxPacket(servo_right);
    //DataPacket servo_right_status = RxPacket();

    printPacketOnLCD(servo_left_status);



    while(1);
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

//ISR para las interrupciones del Timer A0 (Apartado 1 de la practica):
void TA0_0_IRQHandler(void){

        TA0CCTL0 &= ~CCIE; // Interrupt disabled
        TA0CCTL0 &= ~CCIFG; // No interrupt pending

        timer += 1; // Aumentamos el valor del contador del timer

        TA0CCTL0 |= CCIE; // Interrupt enabled

}



void EUSCIA2_IRQHandler(void) { //interrupcion de recepcion en la UART A2
    UCA2IE &= ~UCRXIE; //Interrupciones desactivadas en RX
    uint8_t readData = UCA2RXBUF;
    escribir("Interrupcion", 7);

    if(readIndex == 0 || readIndex == 1){
        if(readData != 0xFF) readingProcessState = 1;
        else escribir("Primer Dato", 7);
    }else if(readIndex == 2){
        readPacket.id = readData;
    }else if(readIndex == 3){
        readPacket.parameterLength = readData;
    }else if(readIndex == 4){
        readPacket.instruction = readData;
    }else if(readIndex > 4 & readIndex < 5 + readPacket.parameterLength){
        readPacket.parametros[readIndex - 5] = readData;
    }else if(readIndex = 5 + readPacket.parameterLength){
        byte bCheckSum = readPacket.id+readPacket.parameterLength+readPacket.instruction;
        byte bCount;
        for(bCount = 0; bCount < readPacket.parameterLength; bCount++) { //Càlcul del checksum
            bCheckSum += readPacket.parametros[bCount];
        }
        if(bCheckSum == readData){
            readingProcessState = 2;
        }
        else{
            readingProcessState = 1;
        }
    }
    readIndex++;

    UCA2IE |= UCRXIE; //Interrupciones reactivadas en RX
}





