#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#define ESPERA 0 //Modo de espera al inicio del juego.
#define INICIO 1 //Todos los LEDs parpadean 2 veces.
#define MOSTRAR 2 //Se muestra la secuencia, en este estado se realizan los ajustes al tiempo
#define REVISAR 3 //Se revisa que el usuario presione los colores correctos.
#define FIN 4 //Todos los LEDs parpadean 3 veces.

#define NOTA1 392 // G4
#define NOTA2 440 // A4
#define NOTA3 493 // B4
#define NOTA4 523 // C5

void parpadear(int n);
void delay(int ms);
void FSM();
void encenderLed(int n);
void imprimirSecuencia();
void iniciarsSecuencia();

void tocarNota(int frecuencia);
void tonadaInicio();
void tonadaError();


volatile int entrada_usuario = -1; //Contiene el botón que se presionó
volatile int overflow_cont;
int enable = 0;
int secuencia[13];
int seed = 137;
int turno = 1;
int estado = ESPERA;
int indice_secuencia = 0;

int main(void)
{
  // Variables
  overflow_cont = 0;
  
  // Timers internos
  TCNT0 = 0x00; // Inicializar Timer0. 
  TCCR0A = 0x00; // Normal 
  TCCR0B = 0b011; // Prescaling 64
  TIMSK = (1 << TOIE0); // Habilitar interrupción por desbordamiento de Timer0

  // Configuración de puertos
  DDRB = 0x0F; // Configurar PB0, PB1, PB2 y PB3 como salidas
  DDRB |= (1 << PB4); // Configurar PB4 como salida
  
  // Configuración de entradas con resistencias pull-up
  DDRA &= ~(1 << PA1); // PA1 como entrada
  PORTA |= (1 << PA1); // Activar pull-up en PA1
  
  DDRD &= ~((1 << PD1) | (1 << PD2) | (1 << PD3)); // PD1, PD2 y PD3 como entradas
  PORTD |= (1 << PD1) | (1 << PD2) | (1 << PD3);   // Activar pull-up en PD1, PD2 y PD3

  // Configuración de interrupciones
  GIMSK = (1 << PCIE1) | (1 << PCIE2); // Habilitar interrupciones por cambio de pines en los puertos A y D
  PCMSK1 = 0b00000010; // PA1 puede disparar la interrupción
  PCMSK2 = 0b00001110; // PD1, PD2 y PD3 pueden disparar la interrupción
  MCUCR = 0x0A; // Configuración del registro MCUCR

  sei(); // Habilitar interrupciones globales

  while(1){
    FSM();
  }
  //Usar para probar parpadear, borrar en version final
  // while (1) {
  //   parpadear(2);
  //   PORTB = 0x00;  // Apagar PB0, PB1, PB2 y PB3
  //   delay(5000); // Esperar 5s
  //   parpadear(3);
  //   PORTB = 0x00;  // Apagar PB0, PB1, PB2 y PB3
  //   delay(5000); // Esperar 5s
    
  // }

  // Usar para testear encenderLed, borrar luego
  // encenderLed(0);
  // delay(1000);
  // encenderLed(1);
  // delay(1000);
  // encenderLed(2);
  // delay(1000);
  // encenderLed(3);
  // delay(1000);
}   

// PORTB = 0x01; enciende 0
// PORTB = 0x02; enciende 1
// PORTB = 0x04; enciende 2
// PORTB = 0x08; enciende 3

void encenderLed(int n) {
   switch(n) {
       case 0:
           PORTB = 0x01;
           tocarNota(NOTA1);  // LED 0, nota 1
           break;
       case 1:
           PORTB = 0x02;
           tocarNota(NOTA2);  // LED 1, nota 2
           break;
       case 2:
           PORTB = 0x04;
           tocarNota(NOTA3);  // LED 2, nota 3
           break;
       case 3:
           PORTB = 0x08;
           tocarNota(NOTA4);  // LED 3, nota 4
           break;
       default:
           break;
   }
}


void parpadear(int n){
//Recibe el número de parpadeos a realizar
    for (int i = 0; i < n; i++) {
        PORTB = 0x0F;  // Encender PB0, PB1, PB2 y PB3
        delay(1000); // Esperar 1s
        PORTB = 0x00;  // Apagar PB0, PB1, PB2 y PB3
        delay(1000); // Esperar 1s
    }
}

void delay(int ms) {
    // Cada overflow ocurre después de (256 * prescaler / F_CPU) segundos
    // Prescaler es 64, F_CPU es 16 MHz
    // Overflow ocurre cada 1024 us, necesitamos 1000 us * ms para ms
    ms = 0.5432103*ms;
    unsigned int overflows_required = (ms * 1000UL) / 1024;
    
    enable = 1; // Activar el contador de overflows
    overflow_cont = 0; // Reiniciar el contador
    TCNT0 = 0x00; // Reiniciar el contador del Timer0
    
    while (enable) {
        // Esperar a que se alcancen los overflows requeridos
        if (overflow_cont >= overflows_required) {
            enable = 0; // Desactivar el contador de overflows
        }
    }
}

ISR(TIMER0_OVF_vect)
{ 
  if (enable){
    overflow_cont++;
  }
}

// ISR para interrupciones en el puerto A (PA1)
ISR(PCINT1_vect) {
    if (!(PINA & (1 << PA1))) {  // Si se presiona PA1
        entrada_usuario = 1;     // Corresponde al LED 1
        PORTB = 0x02;
        tocarNota(NOTA2);
        PORTB = 0x00;
    }
}

// ISR para interrupciones en el puerto D (PD1, PD2, PD3)
ISR(PCINT2_vect) {
    if (!(PIND & (1 << PD1))) {  // Si se presiona PD1
        entrada_usuario = 0;     // Corresponde al LED 0
        PORTB = 0x01;
        tocarNota(NOTA1);
        PORTB = 0x00;
    } else if (!(PIND & (1 << PD2))) {  // Si se presiona PD2
        entrada_usuario = 2;     // Corresponde al LED 2
        PORTB = 0x04;
        tocarNota(NOTA3);
        PORTB = 0x00;
    } else if (!(PIND & (1 << PD3))) {  // Si se presiona PD3
        entrada_usuario = 3;     // Corresponde al LED 3
        PORTB = 0x08;
        tocarNota(NOTA4);
        PORTB = 0x00;
    }
}


void imprimirSecuencia(){
    for (int i = 0; i < 3+turno; i++) {
        encenderLed(secuencia[i]);
        delay(2000 - 200 * (turno - 1)); // 2000 ms inicial, menos 200 ms por cada turno
        PORTB = 0x00;
        delay(200);
    }
    PORTB = 0x00;
}

void iniciarsSecuencia() {
    // semilla
    srand(seed);
    
    for (int i = 0; i < 13; i++) {
        secuencia[i] = rand() % 4;  // valores de 0 to 3
    }
}

void delay_us(unsigned int us) {
    // Aproximación simple para retardo
    while (us--) {
        for (volatile int i = 0; i < 3; i++);
    }
}

void tocarNota(int frecuencia) {
    unsigned long periodo = 1000000UL / frecuencia;  //Frecuencia a periodo en microsegundos
    unsigned long medio_periodo = periodo / 2;  //Mitad del periodo para la frecuencia
    unsigned long ciclos = (200000UL / periodo);  //200 ms de duración en ciclos del buzzer

    for (unsigned long i = 0; i < ciclos; i++) {
        PORTB |= (1 << PB4);  //Encender el buzzer
        delay_us(medio_periodo);  //Esperar la mitad del periodo
        PORTB &= ~(1 << PB4);  //Apagar el buzzer
        delay_us(medio_periodo);  //Esperar la otra mitad del periodo
    }
}

void tonadaInicio() {
    PORTB = 0x01;
    tocarNota(NOTA1);
    PORTB = 0x00;
    _delay_ms(100);
    PORTB = 0x02;
    tocarNota(NOTA2);
    PORTB = 0x00;
    _delay_ms(100);
    PORTB = 0x04;
    tocarNota(NOTA3);
    PORTB = 0x00;
    _delay_ms(100);
    PORTB = 0x08;
    tocarNota(NOTA4);
    PORTB = 0x00;
}

void tonadaError() {
    PORTB = 0x08;
    tocarNota(NOTA4);
    PORTB = 0x00;
    _delay_ms(100);
    PORTB = 0x04;
    tocarNota(NOTA3);
    PORTB = 0x00;
    _delay_ms(100);
    PORTB = 0x02;
    tocarNota(NOTA2);
    PORTB = 0x00;
    _delay_ms(100);
    PORTB = 0x01;
    tocarNota(NOTA1);
}

void FSM() {
    switch(estado) {
        case ESPERA:
            if (entrada_usuario != -1) {  // Si se presiona algún botón
                entrada_usuario = -1;
                estado = INICIO;
            }
            break;

        case INICIO:
            tonadaInicio();
            parpadear(2);
            iniciarsSecuencia();  // Generar la secuencia de LEDs
            turno = 1;  // Reiniciar el turno
            estado = MOSTRAR;
            break;

        case MOSTRAR:
            imprimirSecuencia();
            indice_secuencia = 0;  // Reiniciar el índice de la secuencia para revisión
            estado = REVISAR;
            break;

        case REVISAR:
            if (entrada_usuario != -1) {  // Si el usuario presiona un botón
                if (entrada_usuario == secuencia[indice_secuencia]) {  // Comparar con la secuencia
                    indice_secuencia++;
                    entrada_usuario = -1;  // Resetear la entrada del usuario

                    if (indice_secuencia == 3 + turno) {  // Si el usuario completó la secuencia
                        turno++;
                        if (turno == 14) {  // Si llegó al final del juego
                            estado = FIN;
                        } else {
                            estado = MOSTRAR;  // Mostrar la siguiente secuencia
                        }
                    }
                } else {
                    estado = FIN;  // Si el usuario se equivoca, termina el juego
                }
            }
            break;

        case FIN:
            tonadaError();
            parpadear(3);  // Parpadear LEDs 3 veces
            entrada_usuario = -1;
            estado = ESPERA;  // Reiniciar el estado a ESPERA
            break;

        default:
            break;
    }
}