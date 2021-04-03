/*Configuracao do timer e sinal PWM:
O programa deve fazer com que o led conectado a placa troque de estado a cada 1 segundo. Alem disso, um led conectado a placa deve funcionar como um dimmer, acendendo aos poucos ate atingir um maximo de intensidade em 1 segundo e depois apagar aos poucos, atingindo uma intensidade minima em 1 segundo.
Para o segundo caso, utiliza-se um sinal PWM, no qual a intensidade e controlada pela largura de um pulso. Essa largura e determinada pelo tempo que o sinal permanece em nivel alto durante um periodo.
Para o led conectado, o maximo de intensidade sera quando o sinal permanecer em nivel alto durante todo o pulso, ou seja, a largura do pulso e igual ao periodo, de forma que OCR2B = OCR2A + 1. Ja a intensidade minima sera quando o sinal permanecer em nivel baixo durante todo o pulso.
No modo de operacao fast PWM, e possivel controlar o periodo pelo registrador OCR2A e a largura do pulso pelo registrador OCR2B. Desse modo, uma interrupcao do tipo overflow ocorre toda vez que o contador do temporizador atingir o valor em OCR2A + 1. 
A frequencia do clock e de f = 16 MHz. Alem disso, no modo de operacao normal, o overflow e o envio do sinal top acontece depois de uma contagem de R = 255 do registrador (2^n - 1. Sendo n = 8 e o numero de bits do timer 2).
Assim, o maximo intervalo de tempo que o temporizador consegue medir e dado por:
faixa = P(R + 1)/f = 1024(255 + 1)/16*10^6 = 16,384 ms. 
Nesse cenario, a maxima largura de pulso se daria para um OCR2B = 256. Assim, o periodo com a intensidade do led aumenta/diminui durante 1 segundo seria de T = 1segundo/256. Porem, nesse caso, T = 0.0039, o qual e menor que a frequencia do sinal PWM. Logo, e preciso encontrar outro cenario. 
Sabe-se que na maxima largura do pulso, tem-se OCR2B = OCR2A + 1 = M. Sabe-se tambem que o valor em OCR2B deve ser incrementado a cada ciclo x do temporizador. Desse modo:
1/M = x*M*1024/16*10^6 => xM^2 = 16*10^6/1024 = 15625. 
Portanto, uma das possibilidades para M e x e o par (125, 1). Ja OCR2A = 124.
Utilizando um prescaler de 1024 e uma contagem OCR2A = 124, tem-se o seguinte valor de faixa:
faixa = 1024*(124 + 1)/16*10^6 = 8 ms. 
O valor de contagem de 124 significa que um sinal top sera enviado depois de 125 contagens, ja que esta comeca em zero. 
Desse modo, tem-se um sinal PWM gerado no pino OC2B, de periodo 8ms, que tem o nivel alto do pulso controlado pelo registrador OCR2B, que varia de 0 a 125. Como o sinal ocorre no pino OC2B, o led deve ser conectado a ele, na porta PD3.
Apos o intervalo de tempo de 8ms uma interrupcao do tipo overflow ocorre. A cada interrupcao, o registrador OCR2B deve ser incrementado ou decrementado, de modo que o led aumente ou diminua sua intensidade, por meio da variacao da largura do pulso.
Apos 125 ciclos do temporizador, atinge-se um segundo. Para mapear essa quantidade de ciclos, cria-se a variavel timer. Apos esse intervalo, verifica-se o estado do registrador OCR2B. Se ele atingiu o valor de 125, o led está com sua maxima intensidade, logo ela deve passar a diminuir. Se atingiu zero, ela deve aumentar. Para isso, cria-se a variavel led3, que armazena o valor +1 ou -1, a depender do que deve ocorrer como led. 
Ja em relacao ao led incorporado a placa, para atingir o valor de 1 s, tambem e necessario passar por 125 ciclos do temporizador. Assim, utiliza-se a variavel timer para alterar o estado do led incorporado. Se desligado, ele deve ligar e vice versa. Desse modo, enquanto o led na porta PD3 aumenta sua intensidade, o led esta ligado. E o contrario ocorre equanto o led3 diminui sua intensidade.
*/

/* Define a frequencia do clock. */
#define F_CPU 16000000UL 

/* Bibliotecas necessarias para o funcionamento do programa. A primeira e uma biblioteca basica e a segunda, de interrupções. */
#include <avr/io.h>
#include <avr/interrupt.h>

unsigned char *ocr2a; /* Ponteiro associado ao registrador OCR2A, output compare register, do timer 2, que armareza o valor do periodo do sinal PWM. */
int *ocr2b; /* Ponteiro associado ao registrador OCR2B, output compare register, do timer 2, que armareza o valor da largura do pulso do sinal PWM. */
unsigned char *timsk2; /* Ponteiro associado ao registrador TIMSK2, Timer Interrupt Mask Register, do timer 2, para setar interrupcoes. */
unsigned char *tccr2b; /* Ponteiro associado ao registrador TCCR2B, Timer/Counter Control Register B, do timer 2, que seta os pinos de comparacao e define o modo de operacao. */
unsigned char *tccr2a; /* Ponteiro associado ao registrador TCCR2A, Timer/Counter Control Register, do timer 2, que define o modo de operacao e o prescaler. */
unsigned char *p_portb; /* Ponteiros associados aos registradores PORTB */
unsigned char *p_ddrb; 
unsigned char *p_portd; /* Ponteiros associados aos registradores PORTD */
unsigned char *p_ddrd;

volatile int led3; /* Variavel para alterar o estado do led3. Se este deve diminuir a intensidade, a variavel armazena o valor -1. Caso contrario, armazena o valor +1. */
volatile int timer; /* Variavel de contagem. Conta a quantidade de ciclos do temporizador que ocorrem antes de mudar o estado dos leds. */

/* Funcao que configura os perifericos. */
void setup() {
  
  cli(); /* Desabilita todas as interrupcoes para que elas nao ocorram sem que os perifericos estejam configurados. */

  /* Atribui ao ponteiro o endereco do registrador OCR2A. */
  ocr2a = (unsigned char *)0xB3;
  /* O registrador armazena o valor em binario do periodo. No caso, 124. Em binario, 01111100. */
  *ocr2a = 0b01111100;
  /* Atribui ao ponteiro o endereco do registrador OCR2B. */
  ocr2b = (int *)0xB4;
  /* O registrador armazena o valor em binario do valor de largura de pulso para a intensidade mínima. Logo, OCR2B = 0x0. */
  *ocr2b = 0;  
  /* Atribui ao ponteiro o endereco do registrador TIMSK2. */
  timsk2 = (unsigned char *)0x70;
  /*O registrador e configurado da seguinte forma:
  7, 6, 5, 4, 3: bits reservados. nao sao setados. 
  2: bit OCIE2B. Habilita/desabilita a interrupcao de comparacao com o registrador B. Para desabilita-la, e setado como zero. 
  1: bit OCIE2A. Habilita/desabilita a interrupcao de comparacao com o registrador A. Para desabilita-la, e setado como zero.
  0: bit TOIE2. Habilita/desabilita a interrupcao de overflow. Para habilita-la, e setado como um.
  Portanto, tem-se a configuracao: xxxxx001. */
  /* Atribui ao ponteiro o endereco do registrador TIMSK2. */
  *timsk2 &= 0xF9; /* Reseta os bits que devem ser iguais a zero, usando o comparativo 'e'. */
  *timsk2 |= 0x1; /* Seta o bit que deve ser setado como um, usando o comparativo 'ou'. */
  /* Atribui ao ponteiro o endereco do registrador TCCR2B. */
  tccr2b = (unsigned char *)0xB1;
  /* O registrador e configurado da seguinte forma: 
  7, 6: bits associados ao modo de force output compare. Para desabilitar esse modo, os bits sao setados como zero. 
  5, 4: bits reservados. não sao setados.
  3: bit WGM22. Como sera dito na configuracao do registrador TCCR2A, esta associado ao modo de operacao e e setado como 1. 
  2, 1, 0: bits CS22, CS21, CS20. Definem o valor a ser usado no prescaler. No caso, 1024. Para tanto, sao setados como 111.
  Portanto, tem-se a seguinte configuracao: 00xx1111. */
  *tccr2b &= 0x3F; /* Reseta os bits que devem ser iguais a zero, usando o comparativo 'e'. */
  *tccr2b |= 0xF; /* Seta o bit que deve ser setado como um, usando o comparativo 'ou'. */  
  /* Atribui ao ponteiro o endereco do registrador TCCR2A. */
  tccr2a = (unsigned char *)0xB0;
  /* O registrador e configurado da seguinte forma:
  7, 6, 5, 4: bits que controlam o pino de output compare. Como o pino OC2A nao e usado, 7 e 6 sao setados como zero para desconectar os pinos. Ja no caso dos bits 5 e 4, eles definem o modo de operacao do pino OC2B. Deseja-se que ele seja resetado quando ocorre o Compare Match e setado no nivel baixo. Assim, os bits sao setados como 10.
  3, 2: bits reservados. não sao setados.
  1, 0: WGM21 e WGM20: junto ao bit WGM22, no registrador TCCR2B, definem o modo de operacao. Para o modo de operacao fast PWM, com o sinal TOP ocorrendo quando e atingido o valor em OCR2A, os bits sao setados como 111.
  Portanto, tem-se a seguinte configuracao: 0010xx11 */
  *tccr2a &= 0x2F; /* Reseta os bits que devem ser iguais a zero, usando o comparativo 'e'. */
  *tccr2a |= 0x23; /* Seta o bit que deve ser setado como um, usando o comparativo 'ou'. */
  /* Atribui aos ponteiros os endereços dos registradores PORTB */
  p_portb = (unsigned char *)0x25; 
  p_ddrb = (unsigned char *)0x24;
  /* Seta como zero o bit 5 do registrador portb. Dessa forma, ele inicia ligado, para acompanhar o processo de fade in do led conectado a placa. */
  *p_portb |= 0x20;
  /* Seta como um o bit 5 do registrador ddrb. Dessa forma, ele e configurado como saida. */
  *p_ddrb |= 0x20;
  /* Atribui aos ponteiros os endereços dos registradores PORTD*/
  p_portd = (unsigned char *)0x2B; 
  p_ddrd = (unsigned char *)0x2A;
  /* Seta como zero o bit 3 do registrador portd. Dessa forma, ele inicia desligado. */
  *p_portd &= ~0x08;
  /* Seta como um o bit 3 do registrador ddrd. Dessa forma, ele e configurado como saida. */
  *p_ddrd |= 0x08;  

}

/* Rotina de interrupcao executada se ocorre um overflow no timer 2. No caso do modo fast PWM com top em OCR2A, ela vai ocorrer a cada periodo determinado pelo registrador OCR2A. */
ISR (TIMER2_OVF_vect) {

  timer++; /* Incrementa a variavel que conta a quantidade de ciclos a cada periodo do sinal PWM. */
 
  *ocr2b += led3; /* Incrementa ao registrador OCR2B o valor armazenado na variavel led3. */

  
  if (timer == 125) { /* Apos 125 ciclos, tem-se um tempo igual a 125*0.008 = 1 segundo. */

    timer = 0; /* A variavel e resetada para que a contagem inicie novamente. */
    
    if (*ocr2b >= 125) { /* Nessa situacao, OCR2B = OCR2A + 1. Desse modo, o led acende com maxima intensidade. */
      led3 = -1; /* Quando o led atinge seu maximo, a largura de pulso passa a ser diminuida a cada periodo do sinal PWM. Por isso, a variavel armazena o valor -1. */
      *p_portb &= ~0x20; /* Durante o processo do led3 diminuir sua intensidade, o led incorporado deve ficar apagado. Assim, apaga-se o led no inicio do processo. */
    }
    else if (*ocr2b <= 0){ /* Nessa situacao, a largura do pulso e minima e o led tem minima intensidade. */
      led3 = 1; /* Quanto o led atinge seu minimo, a largura de pulso passa a ser aumentada a cada periodo do sinal PWM. Por isso, a variavel armazena o valor +1. */
      *p_portb |= 0x20; /* Durante o processo do led3 aumentar sua intensidade, o led incorporado deve ficar aceso. Assim, acende-se o led no inicio do processo. */
    }
    }
  
  }

int main () {

  setup(); /* Executa a funcao que configura os perifericos. */
  led3 = 1; /* Inicialmente, o led incorporado esta apagado e vai aumentar sua intensidade. Por isso, led3 armazena o valor +1. */
  timer = 0; /* Comeca a contagem de ciclos em zero. */
  sei(); /* Habilita as interrupcoes. */

  while (1) { /*Looping infinito. */
    
    
    }

    return 0;
}
