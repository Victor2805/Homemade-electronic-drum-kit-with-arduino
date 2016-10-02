#define NUM_PIEZOS 8
#define CAJA_THRESHOLD 20  //anything < TRIGGER_THRESHOLD is treated as 0
#define LTOM_THRESHOLD 50
#define RTOM_THRESHOLD 50
#define LCYM_THRESHOLD 100
#define RCYM_THRESHOLD 100
#define BTOM_THRESHOLD 20
#define HIHAT_THRESHOLD 80
#define BOMB_THRESHOLD 20
#define PRIMER_ANALOG 0    

#define CAJA_NOTA 38
#define LTOM_NOTA 48
#define RTOM_NOTA 47
#define LCYM_NOTA 49
#define RCYM_NOTA 51
#define BTOM_NOTA 43
#define HIHAT_NOTA 42
#define BOMB_NOTA 36
//#define HIHAT2_NOTA 46

#define NOTA_ON_CMD 0x90   //The NOTE ON message is sent when the performer hits a key of the music keyboard. It contains parameters to specify the pitch of the note as well as the velocity (intensity of the note when it is hit).                                
#define NOTA_OFF_CMD 0x80  //When a synthesizer receives this message, it starts playing that note with the correct pitch and force level.When the NOTE OFF message is received, the corresponding note is switched off by the synthesizer.
#define MAX_MIDI_VELOCIDAD 127

#define VELOCIDAD_SERIAL 31250

#define SIGNAL_BUFFER_SIZE 100 //buffer=almacenar data temporal
#define PEAK_BUFFER_SIZE 30
#define MAX_TIEMPO_ENTRE_PICOS 20
#define MIN_TIEMPO_ENTRE_PICOS 50


//#define HIHAT_POS 0


int pinLDR = 9;
int Ledpin = 13;
int thresholdLDR = 350;
int valorLDR = 0;

unsigned short MapaAnalog[NUM_PIEZOS];

unsigned short MapaNotas[NUM_PIEZOS];

unsigned short MapaThreshold[NUM_PIEZOS];

//llama buffers para guardar señales analogicas y picos
short indexActualSignal[NUM_PIEZOS];
short indexActualPico[NUM_PIEZOS];
unsigned short BufferSignal[NUM_PIEZOS][SIGNAL_BUFFER_SIZE];
unsigned short BufferPico[NUM_PIEZOS][PEAK_BUFFER_SIZE];

boolean notaLista[NUM_PIEZOS];
unsigned short VelocidadNotaLista[NUM_PIEZOS];
boolean ultPicoCero[NUM_PIEZOS];

unsigned long ultTiempoPico[NUM_PIEZOS];
unsigned long ultTiempoNota[NUM_PIEZOS];

void setup()
{
  Serial.begin(VELOCIDAD_SERIAL);
  
  //initialize globals
  for(short i=0; i<NUM_PIEZOS; ++i)
  {
    indexActualSignal[i] = 0;
    indexActualPico[i] = 0;
    memset(BufferSignal[i],0,sizeof(BufferSignal[i]));
    memset(BufferPico[i],0,sizeof(BufferPico[i]));
    notaLista[i] = false;
    VelocidadNotaLista[i] = 0;
    ultPicoCero[i] = true;
    ultTiempoPico[i] = 0;
    ultTiempoNota[i] = 0;    
    MapaAnalog[i] = PRIMER_ANALOG + i;
  }
  
  MapaThreshold[0] = CAJA_THRESHOLD; 
  MapaThreshold[1] = RTOM_THRESHOLD;
  MapaThreshold[2] = LTOM_THRESHOLD;
  MapaThreshold[3] = RCYM_THRESHOLD;
  MapaThreshold[4] = LCYM_THRESHOLD; 
  MapaThreshold[5] = BTOM_THRESHOLD; 
  MapaThreshold[6] = HIHAT_THRESHOLD;
  MapaThreshold[7] = BOMB_THRESHOLD;

  MapaNotas[0] = CAJA_NOTA;  //noteMap
  MapaNotas[1] = RTOM_NOTA;
  MapaNotas[2] = LTOM_NOTA;
  MapaNotas[3] = RCYM_NOTA;
  MapaNotas[4] = LCYM_NOTA;
  MapaNotas[5] = BTOM_NOTA;  
  MapaNotas[6] = HIHAT_NOTA;
  MapaNotas[7] = BOMB_NOTA;
 
 {//to check if it works -photocell
  pinMode (pinLDR, INPUT);
  pinMode (Ledpin, OUTPUT);
 }
   
}
 

void loop()
{
  unsigned long TiempoActual = millis();
   
  for(short i=0; i<NUM_PIEZOS; ++i)
  {
    //get a new signal from analog read
    unsigned short nuevaSignal = analogRead(MapaAnalog[i]);
    BufferSignal[i][indexActualSignal[i]] = nuevaSignal;
    
    //if new signal is 0
    if(nuevaSignal < MapaThreshold[i])
    {
      if(!ultPicoCero[i] && (TiempoActual - ultTiempoPico[i]) > MAX_TIEMPO_ENTRE_PICOS)
      {
        grabanuevoPico(i,0);
      }
      else
      {
        //get previous signal
        short indexSignalprevio = indexActualSignal[i]-1;
        if(indexSignalprevio < 0) indexSignalprevio = SIGNAL_BUFFER_SIZE-1;        
        unsigned short Signalprevia = BufferSignal[i][indexSignalprevio];
        
        unsigned short nuevoPico = 0;
        
        //find the wave peak if previous signal was not 0 by going
        //through previous signal values until another 0 is reached
        while(Signalprevia >= MapaThreshold[i])
        {
          if(BufferSignal[i][indexSignalprevio] > nuevoPico)
          {
            nuevoPico = BufferSignal[i][indexSignalprevio];        
          }
          
          //decrement previous signal index, and get previous signal
          indexSignalprevio--;
          if(indexSignalprevio < 0) indexSignalprevio = SIGNAL_BUFFER_SIZE-1;
          Signalprevia = BufferSignal[i][indexSignalprevio];
        }
        
        if(nuevoPico > 0)
        {
          grabanuevoPico(i, nuevoPico);
        }
      }
  
    }
        
    indexActualSignal[i]++;
    if(indexActualSignal[i] == SIGNAL_BUFFER_SIZE) indexActualSignal[i] = 0;
   
  }
  int input = analogRead(pinLDR);
  if (input > thresholdLDR) {
    digitalWrite(Ledpin, LOW);
  }
  else {
    digitalWrite(Ledpin, HIGH);
  }
}

void grabanuevoPico(short slot, short nuevoPico)
{
  ultPicoCero[slot] = (nuevoPico == 0);
  
  unsigned long TiempoActual = millis();
  ultTiempoPico[slot] = TiempoActual;
  
  //new peak recorded (newPeak)
  BufferPico[slot][indexActualPico[slot]] = nuevoPico;
  
  //1 of 3 cases can happen:
  // 1) note ready - if new peak >= previous peak
  // 2) note fire - if new peak < previous peak and previous peak was a note ready
  // 3) no note - if new peak < previous peak and previous peak was NOT note ready
  
  //get previous peak
  short IndexPicoprevio = indexActualPico[slot]-1;
  if(IndexPicoprevio < 0) IndexPicoprevio = PEAK_BUFFER_SIZE-1;        
  unsigned short Picoprevio = BufferPico[slot][IndexPicoprevio];
   
  if(nuevoPico > Picoprevio && (TiempoActual - ultTiempoNota[slot]) > MIN_TIEMPO_ENTRE_PICOS)
  {
    notaLista[slot] = true;
    if(nuevoPico > VelocidadNotaLista[slot])
      VelocidadNotaLista[slot] = nuevoPico;
  }
  else if(nuevoPico < Picoprevio && notaLista[slot])
    {
    EnviaNota(MapaNotas[slot], VelocidadNotaLista[slot]);
    notaLista[slot] = false;
    VelocidadNotaLista[slot] = 0;
    ultTiempoNota[slot] = TiempoActual;
    
  }
  
  indexActualPico[slot]++;
  if(indexActualPico[slot] == PEAK_BUFFER_SIZE) indexActualPico[slot] = 0;  
}

void EnviaNota(unsigned short nota, unsigned short velocidad)
{
  if(velocidad > MAX_MIDI_VELOCIDAD)
    velocidad = MAX_MIDI_VELOCIDAD;
  
  midiNotaOn(nota, velocidad);
  midiNotaOff(nota, velocidad);
}

void midiNotaOn(byte nota, byte Velocidadmidi)
{
  if (nota == 42 && analogRead(pinLDR) < thresholdLDR)
  { 
    
  Serial.write(NOTA_ON_CMD);
  Serial.write(46);
  Serial.write(Velocidadmidi);
   }
     else
   {
    
  Serial.write(NOTA_ON_CMD);
  Serial.write(nota);
  Serial.write(Velocidadmidi);
    
    }
  
}

void midiNotaOff(byte nota, byte Velocidadmidi)
{
  Serial.write(NOTA_OFF_CMD);
  Serial.write(nota);
  Serial.write(Velocidadmidi);
}




