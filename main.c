#include <msp430.h> 

#define TimerA_constant 12000

unsigned int avg;
signed int startValue, currentValue;
unsigned char i, j, activityFlag, signalFalling, signalRising;

volatile signed int savedSamples[20];

void Clock_config(void){
    BCSCTL2 = SELM_0 | DIVM_0 | DIVS_0; //SELM_0 - Master Clock DCOCLK

    if (CALBC1_1MHZ != 0xFF) {
            /* Follow recommended flow. First, clear all DCOx and MODx bits. Then
             * apply new RSELx values. Finally, apply new DCOx and MODx bit values.
             */
           DCOCTL = 0x00;
           BCSCTL1 = CALBC1_1MHZ;      /* Set DCO to 1MHz */
           DCOCTL = CALDCO_1MHZ;
    }

    BCSCTL1 |= XT2OFF; // XT2OFF - turn off XT2 oscillator, DIVA_3 - divide ACLK by 8 - 1.5kHz
    BCSCTL3 = XT2S_0 | LFXT1S_2 | XCAP_1; // ACLK = VLO
}


void ADC10_config(void){
    ADC10CTL0 &= ~ENC;

    ADC10AE0 |= BIT4;

    ADC10CTL0 = ADC10SR + SREF_0 + ADC10ON;
    ADC10CTL1 = INCH_4 + CONSEQ_0 + ADC10SSEL_1;

    ADC10CTL0 |= ENC;
}


void Timer_config(void){
    TACTL = TASSEL_1; //TACLK = ACLK = VLO
    TACCTL0 = CCIE; // capture compare interrupt enable
    TACCR0 = TimerA_constant; // 12000/12000Hz = 1s
    TACTL |= MC_1; //Up mode, TA start
}

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer
    activityFlag = 0;

    P1DIR = BIT0 + BIT6;
    P1OUT &= ~(BIT0 + BIT6);
    Clock_config();
    ADC10_config();

    startValue = 0;
    for(i = 0; i < 255; i++){
        for(j = 0; j < 255; j++);
    }
    for(i = 0; i < 5; i++){
        ADC10CTL0 |= ADC10SC; // start ADC - single channel no repeat
        while((ADC10BUSY & ADC10CTL0));
        startValue += ADC10MEM; // add conversion result
    }
    startValue = startValue / 5; // average out conversion results

    Timer_config();
    __bis_SR_register(LPM3_bits + GIE);
    while(1);
    return 0;
}

#pragma vector = ADC10_VECTOR
__interrupt void ADC10(void){

}

#pragma vector = TIMERA0_VECTOR
__interrupt void Timer_A(void){
    TACTL &= ~MC_1; // stop timer

    P1OUT ^= BIT0;

    ADC10CTL0 |= ADC10SC;
    while((ADC10BUSY & ADC10CTL0));
    currentValue = ADC10MEM;

    if(currentValue - startValue > 10 || currentValue - startValue < -10){
        P1OUT ^= BIT6;
        __bic_SR_register(LPM3_bits); //turn on CPU
        //activity detected
        //reconfigure ADC10 for slow sampling

        ADC10CTL0 &= ~ENC;
        BCSCTL1 |= DIVA_1; //slow down ACLK by 2 -> 6000 Hz
        ADC10CTL0 |= ADC10SHT_3; // slow down sampling rate to 6000/(13 ADC10CLK + 64 SHT) = 77 Hz
        //ADC10DTC0 |= ADC10CT;
        //ADC10DTC1 = 20;
        //ADC10SA = (unsigned int) &savedSamples[0]

        ADC10CTL0 |= ENC;

        signalFalling = 0;
        signalRising = 0;
        // we measure 4 samples, average them out and store them as a single sample because of lacking memory
        for(i = 0; i < 20; i++){

            avg = 0;
            for(j = 0; j < 6; j++){
                ADC10CTL0 |= ADC10SC;
                while((ADC10BUSY & ADC10CTL0));
                avg += ADC10MEM;
            }
            avg = avg/6;
            avg -= startValue;
            savedSamples[i] = avg;

            if(i > 0 && avg < savedSamples[i-1]){
                signalFalling++; // counts number of times signal was falling
            } else if (i > 0 && avg > savedSamples[i-1]) {
                signalRising++; // counts number of times signal was rising
            }
        }
        ADC10CTL0 &= ~ENC;
        ADC10CTL0 &= ~ADC10SHT_3;
        BCSCTL1 &= ~DIVA_1;
        ADC10CTL0 |= ENC;

        __bis_SR_register_on_exit(LPM3_bits); //

    } else {

    }

    P1OUT ^= BIT0;
    TACTL |= MC_1;

}
