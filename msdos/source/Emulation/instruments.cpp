struct FMInstrument {
    unsigned char modChar1;
    unsigned char carChar1;
    unsigned char modChar2;
    unsigned char carChar2;
    unsigned char modChar3;
    unsigned char carChar3;
    unsigned char modChar4;
    unsigned char carChar4;
    unsigned char modChar5;
    unsigned char carChar5;
    unsigned char fbConn;
    unsigned char percNote;
};

// Global variable declarations
struct FMInstrument adl[181];


// Initialize FM instrument data
void initFMInstruments() {
    // This function initializes the FM instrument data array
    // with 180 FM instrument definitions from the original QBasic code
    
    // GM1: Acoustic Grand Piano
    adl[0].modChar1 = 1;   adl[0].carChar1 = 1;
    adl[0].modChar2 = 143; adl[0].carChar2 = 6;
    adl[0].modChar3 = 242; adl[0].carChar3 = 242;
    adl[0].modChar4 = 244; adl[0].carChar4 = 247;
    adl[0].modChar5 = 0;   adl[0].carChar5 = 0;
    adl[0].fbConn = 56;    adl[0].percNote = 0;
    
    // GM2: Bright Acoustic Grand
    adl[1].modChar1 = 1;   adl[1].carChar1 = 1;
    adl[1].modChar2 = 75;  adl[1].carChar2 = 0;
    adl[1].modChar3 = 242; adl[1].carChar3 = 242;
    adl[1].modChar4 = 244; adl[1].carChar4 = 247;
    adl[1].modChar5 = 0;   adl[1].carChar5 = 0;
    adl[1].fbConn = 56;    adl[1].percNote = 0;
    
    // GM3: Electric Grand Piano
    adl[2].modChar1 = 1;   adl[2].carChar1 = 1;
    adl[2].modChar2 = 73;  adl[2].carChar2 = 0;
    adl[2].modChar3 = 242; adl[2].carChar3 = 242;
    adl[2].modChar4 = 244; adl[2].carChar4 = 246;
    adl[2].modChar5 = 0;   adl[2].carChar5 = 0;
    adl[2].fbConn = 56;    adl[2].percNote = 0;
    
    // GM4: Honky-tonk Piano
    adl[3].modChar1 = 129; adl[3].carChar1 = 65;
    adl[3].modChar2 = 18;  adl[3].carChar2 = 0;
    adl[3].modChar3 = 242; adl[3].carChar3 = 242;
    adl[3].modChar4 = 247; adl[3].carChar4 = 247;
    adl[3].modChar5 = 0;   adl[3].carChar5 = 0;
    adl[3].fbConn = 54;    adl[3].percNote = 0;
    
    // GM5: Rhodes Piano
    adl[4].modChar1 = 1;   adl[4].carChar1 = 1;
    adl[4].modChar2 = 87;  adl[4].carChar2 = 0;
    adl[4].modChar3 = 241; adl[4].carChar3 = 242;
    adl[4].modChar4 = 247; adl[4].carChar4 = 247;
    adl[4].modChar5 = 0;   adl[4].carChar5 = 0;
    adl[4].fbConn = 48;    adl[4].percNote = 0;
    
    // GM6: Chorused Piano
    adl[5].modChar1 = 1;   adl[5].carChar1 = 1;
    adl[5].modChar2 = 147; adl[5].carChar2 = 0;
    adl[5].modChar3 = 241; adl[5].carChar3 = 242;
    adl[5].modChar4 = 247; adl[5].carChar4 = 247;
    adl[5].modChar5 = 0;   adl[5].carChar5 = 0;
    adl[5].fbConn = 48;    adl[5].percNote = 0;
    
    // GM7: Harpsichord
    adl[6].modChar1 = 1;   adl[6].carChar1 = 22;
    adl[6].modChar2 = 128; adl[6].carChar2 = 14;
    adl[6].modChar3 = 161; adl[6].carChar3 = 242;
    adl[6].modChar4 = 242; adl[6].carChar4 = 245;
    adl[6].modChar5 = 0;   adl[6].carChar5 = 0;
    adl[6].fbConn = 56;    adl[6].percNote = 0;
    
    // GM8: Clavinet
    adl[7].modChar1 = 1;   adl[7].carChar1 = 1;
    adl[7].modChar2 = 146; adl[7].carChar2 = 0;
    adl[7].modChar3 = 194; adl[7].carChar3 = 194;
    adl[7].modChar4 = 248; adl[7].carChar4 = 248;
    adl[7].modChar5 = 0;   adl[7].carChar5 = 0;
    adl[7].fbConn = 58;    adl[7].percNote = 0;
    
    // GM9: Celesta
    adl[8].modChar1 = 12;  adl[8].carChar1 = 129;
    adl[8].modChar2 = 92;  adl[8].carChar2 = 0;
    adl[8].modChar3 = 246; adl[8].carChar3 = 243;
    adl[8].modChar4 = 244; adl[8].carChar4 = 245;
    adl[8].modChar5 = 0;   adl[8].carChar5 = 0;
    adl[8].fbConn = 48;    adl[8].percNote = 0;
    
    // GM10: Glockenspiel
    adl[9].modChar1 = 7;   adl[9].carChar1 = 17;
    adl[9].modChar2 = 151; adl[9].carChar2 = 128;
    adl[9].modChar3 = 243; adl[9].carChar3 = 242;
    adl[9].modChar4 = 242; adl[9].carChar4 = 241;
    adl[9].modChar5 = 0;   adl[9].carChar5 = 0;
    adl[9].fbConn = 50;    adl[9].percNote = 0;
    
    // GM11: Music box
    adl[10].modChar1 = 23;  adl[10].carChar1 = 1;
    adl[10].modChar2 = 33;  adl[10].carChar2 = 0;
    adl[10].modChar3 = 84;  adl[10].carChar3 = 244;
    adl[10].modChar4 = 244; adl[10].carChar4 = 244;
    adl[10].modChar5 = 0;   adl[10].carChar5 = 0;
    adl[10].fbConn = 50;    adl[10].percNote = 0;
    
    // GM12: Vibraphone
    adl[11].modChar1 = 152; adl[11].carChar1 = 129;
    adl[11].modChar2 = 98;  adl[11].carChar2 = 0;
    adl[11].modChar3 = 243; adl[11].carChar3 = 242;
    adl[11].modChar4 = 246; adl[11].carChar4 = 246;
    adl[11].modChar5 = 0;   adl[11].carChar5 = 0;
    adl[11].fbConn = 48;    adl[11].percNote = 0;
    
    // GM13: Marimba
    adl[12].modChar1 = 24;  adl[12].carChar1 = 1;
    adl[12].modChar2 = 35;  adl[12].carChar2 = 0;
    adl[12].modChar3 = 246; adl[12].carChar3 = 231;
    adl[12].modChar4 = 246; adl[12].carChar4 = 247;
    adl[12].modChar5 = 0;   adl[12].carChar5 = 0;
    adl[12].fbConn = 48;    adl[12].percNote = 0;
    
    // GM14: Xylophone
    adl[13].modChar1 = 21;  adl[13].carChar1 = 1;
    adl[13].modChar2 = 145; adl[13].carChar2 = 0;
    adl[13].modChar3 = 246; adl[13].carChar3 = 246;
    adl[13].modChar4 = 246; adl[13].carChar4 = 246;
    adl[13].modChar5 = 0;   adl[13].carChar5 = 0;
    adl[13].fbConn = 52;    adl[13].percNote = 0;
    
    // GM15: Tubular Bells
    adl[14].modChar1 = 69;  adl[14].carChar1 = 129;
    adl[14].modChar2 = 89;  adl[14].carChar2 = 128;
    adl[14].modChar3 = 211; adl[14].carChar3 = 163;
    adl[14].modChar4 = 243; adl[14].carChar4 = 243;
    adl[14].modChar5 = 0;   adl[14].carChar5 = 0;
    adl[14].fbConn = 60;    adl[14].percNote = 0;
    
    // GM16: Dulcimer
    adl[15].modChar1 = 3;   adl[15].carChar1 = 129;
    adl[15].modChar2 = 73;  adl[15].carChar2 = 128;
    adl[15].modChar3 = 117; adl[15].carChar3 = 181;
    adl[15].modChar4 = 245; adl[15].carChar4 = 245;
    adl[15].modChar5 = 1;   adl[15].carChar5 = 0;
    adl[15].fbConn = 52;    adl[15].percNote = 0;
    
    // GM17: Hammond Organ
    adl[16].modChar1 = 113; adl[16].carChar1 = 49;
    adl[16].modChar2 = 146; adl[16].carChar2 = 0;
    adl[16].modChar3 = 246; adl[16].carChar3 = 241;
    adl[16].modChar4 = 20;  adl[16].carChar4 = 7;
    adl[16].modChar5 = 0;   adl[16].carChar5 = 0;
    adl[16].fbConn = 50;    adl[16].percNote = 0;
    
    // GM18: Percussive Organ
    adl[17].modChar1 = 114; adl[17].carChar1 = 48;
    adl[17].modChar2 = 20;  adl[17].carChar2 = 0;
    adl[17].modChar3 = 199; adl[17].carChar3 = 199;
    adl[17].modChar4 = 88;  adl[17].carChar4 = 8;
    adl[17].modChar5 = 0;   adl[17].carChar5 = 0;
    adl[17].fbConn = 50;    adl[17].percNote = 0;
    
    // GM19: Rock Organ
    adl[18].modChar1 = 112; adl[18].carChar1 = 177;
    adl[18].modChar2 = 68;  adl[18].carChar2 = 0;
    adl[18].modChar3 = 170; adl[18].carChar3 = 138;
    adl[18].modChar4 = 24;  adl[18].carChar4 = 8;
    adl[18].modChar5 = 0;   adl[18].carChar5 = 0;
    adl[18].fbConn = 52;    adl[18].percNote = 0;
    
    // GM20: Church Organ
    adl[19].modChar1 = 35;  adl[19].carChar1 = 177;
    adl[19].modChar2 = 147; adl[19].carChar2 = 0;
    adl[19].modChar3 = 151; adl[19].carChar3 = 85;
    adl[19].modChar4 = 35;  adl[19].carChar4 = 20;
    adl[19].modChar5 = 1;   adl[19].carChar5 = 0;
    adl[19].fbConn = 52;    adl[19].percNote = 0;
    
    // GM21: Reed Organ
    adl[20].modChar1 = 97;  adl[20].carChar1 = 177;
    adl[20].modChar2 = 19;  adl[20].carChar2 = 128;
    adl[20].modChar3 = 151; adl[20].carChar3 = 85;
    adl[20].modChar4 = 4;   adl[20].carChar4 = 4;
    adl[20].modChar5 = 1;   adl[20].carChar5 = 0;
    adl[20].fbConn = 48;    adl[20].percNote = 0;
    
    // GM22: Accordion
    adl[21].modChar1 = 36;  adl[21].carChar1 = 177;
    adl[21].modChar2 = 72;  adl[21].carChar2 = 0;
    adl[21].modChar3 = 152; adl[21].carChar3 = 70;
    adl[21].modChar4 = 42;  adl[21].carChar4 = 26;
    adl[21].modChar5 = 1;   adl[21].carChar5 = 0;
    adl[21].fbConn = 60;    adl[21].percNote = 0;
    
    // GM23: Harmonica
    adl[22].modChar1 = 97;  adl[22].carChar1 = 33;
    adl[22].modChar2 = 19;  adl[22].carChar2 = 0;
    adl[22].modChar3 = 145; adl[22].carChar3 = 97;
    adl[22].modChar4 = 6;   adl[22].carChar4 = 7;
    adl[22].modChar5 = 1;   adl[22].carChar5 = 0;
    adl[22].fbConn = 58;    adl[22].percNote = 0;
    
    // GM24: Tango Accordion
    adl[23].modChar1 = 33;  adl[23].carChar1 = 161;
    adl[23].modChar2 = 19;  adl[23].carChar2 = 137;
    adl[23].modChar3 = 113; adl[23].carChar3 = 97;
    adl[23].modChar4 = 6;   adl[23].carChar4 = 7;
    adl[23].modChar5 = 0;   adl[23].carChar5 = 0;
    adl[23].fbConn = 54;    adl[23].percNote = 0;
    
    // GM25: Acoustic Guitar1
    adl[24].modChar1 = 2;   adl[24].carChar1 = 65;
    adl[24].modChar2 = 156; adl[24].carChar2 = 128;
    adl[24].modChar3 = 243; adl[24].carChar3 = 243;
    adl[24].modChar4 = 148; adl[24].carChar4 = 200;
    adl[24].modChar5 = 1;   adl[24].carChar5 = 0;
    adl[24].fbConn = 60;    adl[24].percNote = 0;
    
    // GM26: Acoustic Guitar2
    adl[25].modChar1 = 3;   adl[25].carChar1 = 17;
    adl[25].modChar2 = 84;  adl[25].carChar2 = 0;
    adl[25].modChar3 = 243; adl[25].carChar3 = 241;
    adl[25].modChar4 = 154; adl[25].carChar4 = 231;
    adl[25].modChar5 = 1;   adl[25].carChar5 = 0;
    adl[25].fbConn = 60;    adl[25].percNote = 0;
    
    // GM27: Electric Guitar1
    adl[26].modChar1 = 35;  adl[26].carChar1 = 33;
    adl[26].modChar2 = 95;  adl[26].carChar2 = 0;
    adl[26].modChar3 = 241; adl[26].carChar3 = 242;
    adl[26].modChar4 = 58;  adl[26].carChar4 = 248;
    adl[26].modChar5 = 0;   adl[26].carChar5 = 0;
    adl[26].fbConn = 48;    adl[26].percNote = 0;
    
    // GM28: Electric Guitar2
    adl[27].modChar1 = 3;   adl[27].carChar1 = 33;
    adl[27].modChar2 = 135; adl[27].carChar2 = 128;
    adl[27].modChar3 = 246; adl[27].carChar3 = 243;
    adl[27].modChar4 = 34;  adl[27].carChar4 = 248;
    adl[27].modChar5 = 1;   adl[27].carChar5 = 0;
    adl[27].fbConn = 54;    adl[27].percNote = 0;
    
    // GM29: Electric Guitar3
    adl[28].modChar1 = 3;   adl[28].carChar1 = 33;
    adl[28].modChar2 = 71;  adl[28].carChar2 = 0;
    adl[28].modChar3 = 249; adl[28].carChar3 = 246;
    adl[28].modChar4 = 84;  adl[28].carChar4 = 58;
    adl[28].modChar5 = 0;   adl[28].carChar5 = 0;
    adl[28].fbConn = 48;    adl[28].percNote = 0;
    
    // GM30: Overdrive Guitar
    adl[29].modChar1 = 35;  adl[29].carChar1 = 33;
    adl[29].modChar2 = 74;  adl[29].carChar2 = 5;
    adl[29].modChar3 = 145; adl[29].carChar3 = 132;
    adl[29].modChar4 = 65;  adl[29].carChar4 = 25;
    adl[29].modChar5 = 1;   adl[29].carChar5 = 0;
    adl[29].fbConn = 56;    adl[29].percNote = 0;
    
    // GM31: Distortion Guitar
    adl[30].modChar1 = 35;  adl[30].carChar1 = 33;
    adl[30].modChar2 = 74;  adl[30].carChar2 = 0;
    adl[30].modChar3 = 149; adl[30].carChar3 = 148;
    adl[30].modChar4 = 25;  adl[30].carChar4 = 25;
    adl[30].modChar5 = 1;   adl[30].carChar5 = 0;
    adl[30].fbConn = 56;    adl[30].percNote = 0;
    
    // GM32: Guitar Harmonics
    adl[31].modChar1 = 9;   adl[31].carChar1 = 132;
    adl[31].modChar2 = 161; adl[31].carChar2 = 128;
    adl[31].modChar3 = 32;  adl[31].carChar3 = 209;
    adl[31].modChar4 = 79;  adl[31].carChar4 = 248;
    adl[31].modChar5 = 0;   adl[31].carChar5 = 0;
    adl[31].fbConn = 56;    adl[31].percNote = 0;
    
    // GM33: Acoustic Bass
    adl[32].modChar1 = 33;  adl[32].carChar1 = 162;
    adl[32].modChar2 = 30;  adl[32].carChar2 = 0;
    adl[32].modChar3 = 148; adl[32].carChar3 = 195;
    adl[32].modChar4 = 6;   adl[32].carChar4 = 166;
    adl[32].modChar5 = 0;   adl[32].carChar5 = 0;
    adl[32].fbConn = 50;    adl[32].percNote = 0;
    
    // GM34: Electric Bass 1
    adl[33].modChar1 = 49;  adl[33].carChar1 = 49;
    adl[33].modChar2 = 18;  adl[33].carChar2 = 0;
    adl[33].modChar3 = 241; adl[33].carChar3 = 241;
    adl[33].modChar4 = 40;  adl[33].carChar4 = 24;
    adl[33].modChar5 = 0;   adl[33].carChar5 = 0;
    adl[33].fbConn = 58;    adl[33].percNote = 0;
    
    // GM35: Electric Bass 2
    adl[34].modChar1 = 49;  adl[34].carChar1 = 49;
    adl[34].modChar2 = 141; adl[34].carChar2 = 0;
    adl[34].modChar3 = 241; adl[34].carChar3 = 241;
    adl[34].modChar4 = 232; adl[34].carChar4 = 120;
    adl[34].modChar5 = 0;   adl[34].carChar5 = 0;
    adl[34].fbConn = 58;    adl[34].percNote = 0;
    
    // GM36: Fretless Bass
    adl[35].modChar1 = 49;  adl[35].carChar1 = 50;
    adl[35].modChar2 = 91;  adl[35].carChar2 = 0;
    adl[35].modChar3 = 81;  adl[35].carChar3 = 113;
    adl[35].modChar4 = 40;  adl[35].carChar4 = 72;
    adl[35].modChar5 = 0;   adl[35].carChar5 = 0;
    adl[35].fbConn = 60;    adl[35].percNote = 0;
    
    // GM37: Slap Bass 1
    adl[36].modChar1 = 1;   adl[36].carChar1 = 33;
    adl[36].modChar2 = 139; adl[36].carChar2 = 64;
    adl[36].modChar3 = 161; adl[36].carChar3 = 242;
    adl[36].modChar4 = 154; adl[36].carChar4 = 223;
    adl[36].modChar5 = 0;   adl[36].carChar5 = 0;
    adl[36].fbConn = 56;    adl[36].percNote = 0;
    
    // GM38: Slap Bass 2
    adl[37].modChar1 = 33;  adl[37].carChar1 = 33;
    adl[37].modChar2 = 139; adl[37].carChar2 = 8;
    adl[37].modChar3 = 162; adl[37].carChar3 = 161;
    adl[37].modChar4 = 22;  adl[37].carChar4 = 223;
    adl[37].modChar5 = 0;   adl[37].carChar5 = 0;
    adl[37].fbConn = 56;    adl[37].percNote = 0;
    
    // GM39: Synth Bass 1
    adl[38].modChar1 = 49;  adl[38].carChar1 = 49;
    adl[38].modChar2 = 139; adl[38].carChar2 = 0;
    adl[38].modChar3 = 244; adl[38].carChar3 = 241;
    adl[38].modChar4 = 232; adl[38].carChar4 = 120;
    adl[38].modChar5 = 0;   adl[38].carChar5 = 0;
    adl[38].fbConn = 58;    adl[38].percNote = 0;
    
    // GM40: Synth Bass 2
    adl[39].modChar1 = 49;  adl[39].carChar1 = 49;
    adl[39].modChar2 = 18;  adl[39].carChar2 = 0;
    adl[39].modChar3 = 241; adl[39].carChar3 = 241;
    adl[39].modChar4 = 40;  adl[39].carChar4 = 24;
    adl[39].modChar5 = 0;   adl[39].carChar5 = 0;
    adl[39].fbConn = 58;    adl[39].percNote = 0;
    
    // GM41: Violin
    adl[40].modChar1 = 49;  adl[40].carChar1 = 33;
    adl[40].modChar2 = 21;  adl[40].carChar2 = 0;
    adl[40].modChar3 = 221; adl[40].carChar3 = 86;
    adl[40].modChar4 = 19;  adl[40].carChar4 = 38;
    adl[40].modChar5 = 1;   adl[40].carChar5 = 0;
    adl[40].fbConn = 56;    adl[40].percNote = 0;
    
    // GM42: Viola
    adl[41].modChar1 = 49;  adl[41].carChar1 = 33;
    adl[41].modChar2 = 22;  adl[41].carChar2 = 0;
    adl[41].modChar3 = 221; adl[41].carChar3 = 102;
    adl[41].modChar4 = 19;  adl[41].carChar4 = 6;
    adl[41].modChar5 = 1;   adl[41].carChar5 = 0;
    adl[41].fbConn = 56;    adl[41].percNote = 0;
    
    // GM43: Cello
    adl[42].modChar1 = 113; adl[42].carChar1 = 49;
    adl[42].modChar2 = 73;  adl[42].carChar2 = 0;
    adl[42].modChar3 = 209; adl[42].carChar3 = 97;
    adl[42].modChar4 = 28;  adl[42].carChar4 = 12;
    adl[42].modChar5 = 1;   adl[42].carChar5 = 0;
    adl[42].fbConn = 56;    adl[42].percNote = 0;
    
    // GM44: Contrabass
    adl[43].modChar1 = 33;  adl[43].carChar1 = 35;
    adl[43].modChar2 = 77;  adl[43].carChar2 = 128;
    adl[43].modChar3 = 113; adl[43].carChar3 = 114;
    adl[43].modChar4 = 18;  adl[43].carChar4 = 6;
    adl[43].modChar5 = 1;   adl[43].carChar5 = 0;
    adl[43].fbConn = 50;    adl[43].percNote = 0;
    
    // GM45: Tremulo Strings
    adl[44].modChar1 = 241; adl[44].carChar1 = 225;
    adl[44].modChar2 = 64;  adl[44].carChar2 = 0;
    adl[44].modChar3 = 241; adl[44].carChar3 = 111;
    adl[44].modChar4 = 33;  adl[44].carChar4 = 22;
    adl[44].modChar5 = 1;   adl[44].carChar5 = 0;
    adl[44].fbConn = 50;    adl[44].percNote = 0;
    
    // GM46: Pizzicato String
    adl[45].modChar1 = 2;   adl[45].carChar1 = 1;
    adl[45].modChar2 = 26;  adl[45].carChar2 = 128;
    adl[45].modChar3 = 245; adl[45].carChar3 = 133;
    adl[45].modChar4 = 117; adl[45].carChar4 = 53;
    adl[45].modChar5 = 1;   adl[45].carChar5 = 0;
    adl[45].fbConn = 48;    adl[45].percNote = 0;
    
    // GM47: Orchestral Harp
    adl[46].modChar1 = 2;   adl[46].carChar1 = 1;
    adl[46].modChar2 = 29;  adl[46].carChar2 = 128;
    adl[46].modChar3 = 245; adl[46].carChar3 = 243;
    adl[46].modChar4 = 117; adl[46].carChar4 = 244;
    adl[46].modChar5 = 1;   adl[46].carChar5 = 0;
    adl[46].fbConn = 48;    adl[46].percNote = 0;
    
    // GM48: Timpany
    adl[47].modChar1 = 16;  adl[47].carChar1 = 17;
    adl[47].modChar2 = 65;  adl[47].carChar2 = 0;
    adl[47].modChar3 = 245; adl[47].carChar3 = 242;
    adl[47].modChar4 = 5;   adl[47].carChar4 = 195;
    adl[47].modChar5 = 1;   adl[47].carChar5 = 0;
    adl[47].fbConn = 50;    adl[47].percNote = 0;
    
    // GM49: String Ensemble1
    adl[48].modChar1 = 33;  adl[48].carChar1 = 162;
    adl[48].modChar2 = 155; adl[48].carChar2 = 1;
    adl[48].modChar3 = 177; adl[48].carChar3 = 114;
    adl[48].modChar4 = 37;  adl[48].carChar4 = 8;
    adl[48].modChar5 = 1;   adl[48].carChar5 = 0;
    adl[48].fbConn = 62;    adl[48].percNote = 0;
    
    // GM50: String Ensemble2
    adl[49].modChar1 = 161; adl[49].carChar1 = 33;
    adl[49].modChar2 = 152; adl[49].carChar2 = 0;
    adl[49].modChar3 = 127; adl[49].carChar3 = 63;
    adl[49].modChar4 = 3;   adl[49].carChar4 = 7;
    adl[49].modChar5 = 1;   adl[49].carChar5 = 1;
    adl[49].fbConn = 48;    adl[49].percNote = 0;
    
    // GM51: Synth Strings 1
    adl[50].modChar1 = 161; adl[50].carChar1 = 97;
    adl[50].modChar2 = 147; adl[50].carChar2 = 0;
    adl[50].modChar3 = 193; adl[50].carChar3 = 79;
    adl[50].modChar4 = 18;  adl[50].carChar4 = 5;
    adl[50].modChar5 = 0;   adl[50].carChar5 = 0;
    adl[50].fbConn = 58;    adl[50].percNote = 0;
    
    // GM52: SynthStrings 2
    adl[51].modChar1 = 33;  adl[51].carChar1 = 97;
    adl[51].modChar2 = 24;  adl[51].carChar2 = 0;
    adl[51].modChar3 = 193; adl[51].carChar3 = 79;
    adl[51].modChar4 = 34;  adl[51].carChar4 = 5;
    adl[51].modChar5 = 0;   adl[51].carChar5 = 0;
    adl[51].fbConn = 60;    adl[51].percNote = 0;
    
    // GM53: Choir Aahs
    adl[52].modChar1 = 49;  adl[52].carChar1 = 114;
    adl[52].modChar2 = 91;  adl[52].carChar2 = 131;
    adl[52].modChar3 = 244; adl[52].carChar3 = 138;
    adl[52].modChar4 = 21;  adl[52].carChar4 = 5;
    adl[52].modChar5 = 0;   adl[52].carChar5 = 0;
    adl[52].fbConn = 48;    adl[52].percNote = 0;
    
    // GM54: Voice Oohs
    adl[53].modChar1 = 161; adl[53].carChar1 = 97;
    adl[53].modChar2 = 144; adl[53].carChar2 = 0;
    adl[53].modChar3 = 116; adl[53].carChar3 = 113;
    adl[53].modChar4 = 57;  adl[53].carChar4 = 103;
    adl[53].modChar5 = 0;   adl[53].carChar5 = 0;
    adl[53].fbConn = 48;    adl[53].percNote = 0;
    
    // GM55: Synth Voice
    adl[54].modChar1 = 113; adl[54].carChar1 = 114;
    adl[54].modChar2 = 87;  adl[54].carChar2 = 0;
    adl[54].modChar3 = 84;  adl[54].carChar3 = 122;
    adl[54].modChar4 = 5;   adl[54].carChar4 = 5;
    adl[54].modChar5 = 0;   adl[54].carChar5 = 0;
    adl[54].fbConn = 60;    adl[54].percNote = 0;
    
    // GM56: Orchestra Hit
    adl[55].modChar1 = 144; adl[55].carChar1 = 65;
    adl[55].modChar2 = 0;   adl[55].carChar2 = 0;
    adl[55].modChar3 = 84;  adl[55].carChar3 = 165;
    adl[55].modChar4 = 99;  adl[55].carChar4 = 69;
    adl[55].modChar5 = 0;   adl[55].carChar5 = 0;
    adl[55].fbConn = 56;    adl[55].percNote = 0;
    
    // GM57: Trumpet
    adl[56].modChar1 = 33;  adl[56].carChar1 = 33;
    adl[56].modChar2 = 146; adl[56].carChar2 = 1;
    adl[56].modChar3 = 133; adl[56].carChar3 = 143;
    adl[56].modChar4 = 23;  adl[56].carChar4 = 9;
    adl[56].modChar5 = 0;   adl[56].carChar5 = 0;
    adl[56].fbConn = 60;    adl[56].percNote = 0;
    
    // GM58: Trombone
    adl[57].modChar1 = 33;  adl[57].carChar1 = 33;
    adl[57].modChar2 = 148; adl[57].carChar2 = 5;
    adl[57].modChar3 = 117; adl[57].carChar3 = 143;
    adl[57].modChar4 = 23;  adl[57].carChar4 = 9;
    adl[57].modChar5 = 0;   adl[57].carChar5 = 0;
    adl[57].fbConn = 60;    adl[57].percNote = 0;
    
    // GM59: Tuba
    adl[58].modChar1 = 33;  adl[58].carChar1 = 97;
    adl[58].modChar2 = 148; adl[58].carChar2 = 0;
    adl[58].modChar3 = 118; adl[58].carChar3 = 130;
    adl[58].modChar4 = 21;  adl[58].carChar4 = 55;
    adl[58].modChar5 = 0;   adl[58].carChar5 = 0;
    adl[58].fbConn = 60;    adl[58].percNote = 0;
    
    // GM60: Muted Trumpet
    adl[59].modChar1 = 49;  adl[59].carChar1 = 33;
    adl[59].modChar2 = 67;  adl[59].carChar2 = 0;
    adl[59].modChar3 = 158; adl[59].carChar3 = 98;
    adl[59].modChar4 = 23;  adl[59].carChar4 = 44;
    adl[59].modChar5 = 1;   adl[59].carChar5 = 1;
    adl[59].fbConn = 50;    adl[59].percNote = 0;
    
    // GM61: French Horn
    adl[60].modChar1 = 33;  adl[60].carChar1 = 33;
    adl[60].modChar2 = 155; adl[60].carChar2 = 0;
    adl[60].modChar3 = 97;  adl[60].carChar3 = 127;
    adl[60].modChar4 = 106; adl[60].carChar4 = 10;
    adl[60].modChar5 = 0;   adl[60].carChar5 = 0;
    adl[60].fbConn = 50;    adl[60].percNote = 0;
    
    // GM62: Brass Section
    adl[61].modChar1 = 97;  adl[61].carChar1 = 34;
    adl[61].modChar2 = 138; adl[61].carChar2 = 6;
    adl[61].modChar3 = 117; adl[61].carChar3 = 116;
    adl[61].modChar4 = 31;  adl[61].carChar4 = 15;
    adl[61].modChar5 = 0;   adl[61].carChar5 = 0;
    adl[61].fbConn = 56;    adl[61].percNote = 0;
    
    // GM63: Synth Brass 1
    adl[62].modChar1 = 161; adl[62].carChar1 = 33;
    adl[62].modChar2 = 134; adl[62].carChar2 = 131;
    adl[62].modChar3 = 114; adl[62].carChar3 = 113;
    adl[62].modChar4 = 85;  adl[62].carChar4 = 24;
    adl[62].modChar5 = 1;   adl[62].carChar5 = 0;
    adl[62].fbConn = 48;    adl[62].percNote = 0;
    
    // GM64: Synth Brass 2
    adl[63].modChar1 = 33;  adl[63].carChar1 = 33;
    adl[63].modChar2 = 77;  adl[63].carChar2 = 0;
    adl[63].modChar3 = 84;  adl[63].carChar3 = 166;
    adl[63].modChar4 = 60;  adl[63].carChar4 = 28;
    adl[63].modChar5 = 0;   adl[63].carChar5 = 0;
    adl[63].fbConn = 56;    adl[63].percNote = 0;
    
    // GM65: Soprano Sax
    adl[64].modChar1 = 49;  adl[64].carChar1 = 97;
    adl[64].modChar2 = 143; adl[64].carChar2 = 0;
    adl[64].modChar3 = 147; adl[64].carChar3 = 114;
    adl[64].modChar4 = 2;   adl[64].carChar4 = 11;
    adl[64].modChar5 = 1;   adl[64].carChar5 = 0;
    adl[64].fbConn = 56;    adl[64].percNote = 0;
    
    // GM66: Alto Sax
    adl[65].modChar1 = 49;  adl[65].carChar1 = 97;
    adl[65].modChar2 = 142; adl[65].carChar2 = 0;
    adl[65].modChar3 = 147; adl[65].carChar3 = 114;
    adl[65].modChar4 = 3;   adl[65].carChar4 = 9;
    adl[65].modChar5 = 1;   adl[65].carChar5 = 0;
    adl[65].fbConn = 56;    adl[65].percNote = 0;
    
    // GM67: Tenor Sax
    adl[66].modChar1 = 49;  adl[66].carChar1 = 97;
    adl[66].modChar2 = 145; adl[66].carChar2 = 0;
    adl[66].modChar3 = 147; adl[66].carChar3 = 130;
    adl[66].modChar4 = 3;   adl[66].carChar4 = 9;
    adl[66].modChar5 = 1;   adl[66].carChar5 = 0;
    adl[66].fbConn = 58;    adl[66].percNote = 0;
    
    // GM68: Baritone Sax
    adl[67].modChar1 = 49;  adl[67].carChar1 = 97;
    adl[67].modChar2 = 142; adl[67].carChar2 = 0;
    adl[67].modChar3 = 147; adl[67].carChar3 = 114;
    adl[67].modChar4 = 15;  adl[67].carChar4 = 15;
    adl[67].modChar5 = 1;   adl[67].carChar5 = 0;
    adl[67].fbConn = 58;    adl[67].percNote = 0;
    
    // GM69: Oboe
    adl[68].modChar1 = 33;  adl[68].carChar1 = 33;
    adl[68].modChar2 = 75;  adl[68].carChar2 = 0;
    adl[68].modChar3 = 170; adl[68].carChar3 = 143;
    adl[68].modChar4 = 22;  adl[68].carChar4 = 10;
    adl[68].modChar5 = 1;   adl[68].carChar5 = 0;
    adl[68].fbConn = 56;    adl[68].percNote = 0;
    
    // GM70: English Horn
    adl[69].modChar1 = 49;  adl[69].carChar1 = 33;
    adl[69].modChar2 = 144; adl[69].carChar2 = 0;
    adl[69].modChar3 = 126; adl[69].carChar3 = 139;
    adl[69].modChar4 = 23;  adl[69].carChar4 = 12;
    adl[69].modChar5 = 1;   adl[69].carChar5 = 1;
    adl[69].fbConn = 54;    adl[69].percNote = 0;
    
    // GM71: Bassoon
    adl[70].modChar1 = 49;  adl[70].carChar1 = 50;
    adl[70].modChar2 = 129; adl[70].carChar2 = 0;
    adl[70].modChar3 = 117; adl[70].carChar3 = 97;
    adl[70].modChar4 = 25;  adl[70].carChar4 = 25;
    adl[70].modChar5 = 1;   adl[70].carChar5 = 0;
    adl[70].fbConn = 48;    adl[70].percNote = 0;
    
    // GM72: Clarinet
    adl[71].modChar1 = 50;  adl[71].carChar1 = 33;
    adl[71].modChar2 = 144; adl[71].carChar2 = 0;
    adl[71].modChar3 = 155; adl[71].carChar3 = 114;
    adl[71].modChar4 = 33;  adl[71].carChar4 = 23;
    adl[71].modChar5 = 0;   adl[71].carChar5 = 0;
    adl[71].fbConn = 52;    adl[71].percNote = 0;
    
    // GM73: Piccolo
    adl[72].modChar1 = 225; adl[72].carChar1 = 225;
    adl[72].modChar2 = 31;  adl[72].carChar2 = 0;
    adl[72].modChar3 = 133; adl[72].carChar3 = 101;
    adl[72].modChar4 = 95;  adl[72].carChar4 = 26;
    adl[72].modChar5 = 0;   adl[72].carChar5 = 0;
    adl[72].fbConn = 48;    adl[72].percNote = 0;
    
    // GM74: Flute
    adl[73].modChar1 = 225; adl[73].carChar1 = 225;
    adl[73].modChar2 = 70;  adl[73].carChar2 = 0;
    adl[73].modChar3 = 136; adl[73].carChar3 = 101;
    adl[73].modChar4 = 95;  adl[73].carChar4 = 26;
    adl[73].modChar5 = 0;   adl[73].carChar5 = 0;
    adl[73].fbConn = 48;    adl[73].percNote = 0;
    
    // GM75: Recorder
    adl[74].modChar1 = 161; adl[74].carChar1 = 33;
    adl[74].modChar2 = 156; adl[74].carChar2 = 0;
    adl[74].modChar3 = 117; adl[74].carChar3 = 117;
    adl[74].modChar4 = 31;  adl[74].carChar4 = 10;
    adl[74].modChar5 = 0;   adl[74].carChar5 = 0;
    adl[74].fbConn = 50;    adl[74].percNote = 0;
    
    // GM76: Pan Flute
    adl[75].modChar1 = 49;  adl[75].carChar1 = 33;
    adl[75].modChar2 = 139; adl[75].carChar2 = 0;
    adl[75].modChar3 = 132; adl[75].carChar3 = 101;
    adl[75].modChar4 = 88;  adl[75].carChar4 = 26;
    adl[75].modChar5 = 0;   adl[75].carChar5 = 0;
    adl[75].fbConn = 48;    adl[75].percNote = 0;
    
    // GM77: Bottle Blow
    adl[76].modChar1 = 225; adl[76].carChar1 = 161;
    adl[76].modChar2 = 76;  adl[76].carChar2 = 0;
    adl[76].modChar3 = 102; adl[76].carChar3 = 101;
    adl[76].modChar4 = 86;  adl[76].carChar4 = 38;
    adl[76].modChar5 = 0;   adl[76].carChar5 = 0;
    adl[76].fbConn = 48;    adl[76].percNote = 0;
    
    // GM78: Shakuhachi
    adl[77].modChar1 = 98;  adl[77].carChar1 = 161;
    adl[77].modChar2 = 203; adl[77].carChar2 = 0;
    adl[77].modChar3 = 118; adl[77].carChar3 = 85;
    adl[77].modChar4 = 70;  adl[77].carChar4 = 54;
    adl[77].modChar5 = 0;   adl[77].carChar5 = 0;
    adl[77].fbConn = 48;    adl[77].percNote = 0;
    
    // GM79: Whistle
    adl[78].modChar1 = 98;  adl[78].carChar1 = 161;
    adl[78].modChar2 = 153; adl[78].carChar2 = 0;
    adl[78].modChar3 = 87;  adl[78].carChar3 = 86;
    adl[78].modChar4 = 7;   adl[78].carChar4 = 7;
    adl[78].modChar5 = 0;   adl[78].carChar5 = 0;
    adl[78].fbConn = 59;    adl[78].percNote = 0;
    
    // GM80: Ocarina
    adl[79].modChar1 = 98;  adl[79].carChar1 = 161;
    adl[79].modChar2 = 147; adl[79].carChar2 = 0;
    adl[79].modChar3 = 119; adl[79].carChar3 = 118;
    adl[79].modChar4 = 7;   adl[79].carChar4 = 7;
    adl[79].modChar5 = 0;   adl[79].carChar5 = 0;
    adl[79].fbConn = 59;    adl[79].percNote = 0;
    
    // GM81: Lead 1 squareea
    adl[80].modChar1 = 34;  adl[80].carChar1 = 33;
    adl[80].modChar2 = 89;  adl[80].carChar2 = 0;
    adl[80].modChar3 = 255; adl[80].carChar3 = 255;
    adl[80].modChar4 = 3;   adl[80].carChar4 = 15;
    adl[80].modChar5 = 2;   adl[80].carChar5 = 0;
    adl[80].fbConn = 48;    adl[80].percNote = 0;
    
    // GM82: Lead 2 sawtooth
    adl[81].modChar1 = 33;  adl[81].carChar1 = 33;
    adl[81].modChar2 = 14;  adl[81].carChar2 = 0;
    adl[81].modChar3 = 255; adl[81].carChar3 = 255;
    adl[81].modChar4 = 15;  adl[81].carChar4 = 15;
    adl[81].modChar5 = 1;   adl[81].carChar5 = 1;
    adl[81].fbConn = 48;    adl[81].percNote = 0;
    
    // GM83: Lead 3 calliope
    adl[82].modChar1 = 34;  adl[82].carChar1 = 33;
    adl[82].modChar2 = 70;  adl[82].carChar2 = 128;
    adl[82].modChar3 = 134; adl[82].carChar3 = 100;
    adl[82].modChar4 = 85;  adl[82].carChar4 = 24;
    adl[82].modChar5 = 0;   adl[82].carChar5 = 0;
    adl[82].fbConn = 48;    adl[82].percNote = 0;
    
    // GM84: Lead 4 chiff
    adl[83].modChar1 = 33;  adl[83].carChar1 = 161;
    adl[83].modChar2 = 69;  adl[83].carChar2 = 0;
    adl[83].modChar3 = 102; adl[83].carChar3 = 150;
    adl[83].modChar4 = 18;  adl[83].carChar4 = 10;
    adl[83].modChar5 = 0;   adl[83].carChar5 = 0;
    adl[83].fbConn = 48;    adl[83].percNote = 0;

    // GM85: Lead 5 charang
    adl[84].modChar1 = 33;  adl[84].carChar1 = 34;
    adl[84].modChar2 = 139; adl[84].carChar2 = 0;
    adl[84].modChar3 = 146; adl[84].carChar3 = 145;
    adl[84].modChar4 = 42;  adl[84].carChar4 = 42;
    adl[84].modChar5 = 1;   adl[84].carChar5 = 0;
    adl[84].fbConn = 48;    adl[84].percNote = 0;
    
    // GM86: Lead 6 voice
    adl[85].modChar1 = 162; adl[85].carChar1 = 97;
    adl[85].modChar2 = 158; adl[85].carChar2 = 64;
    adl[85].modChar3 = 223; adl[85].carChar3 = 111;
    adl[85].modChar4 = 5;   adl[85].carChar4 = 7;
    adl[85].modChar5 = 0;   adl[85].carChar5 = 0;
    adl[85].fbConn = 50;    adl[85].percNote = 0;
    
    // GM87: Lead 7 fifths
    adl[86].modChar1 = 32;  adl[86].carChar1 = 96;
    adl[86].modChar2 = 26;  adl[86].carChar2 = 0;
    adl[86].modChar3 = 239; adl[86].carChar3 = 143;
    adl[86].modChar4 = 1;   adl[86].carChar4 = 6;
    adl[86].modChar5 = 0;   adl[86].carChar5 = 2;
    adl[86].fbConn = 48;    adl[86].percNote = 0;
    
    // GM88: Lead 8 brass
    adl[87].modChar1 = 33;  adl[87].carChar1 = 33;
    adl[87].modChar2 = 143; adl[87].carChar2 = 128;
    adl[87].modChar3 = 241; adl[87].carChar3 = 244;
    adl[87].modChar4 = 41;  adl[87].carChar4 = 9;
    adl[87].modChar5 = 0;   adl[87].carChar5 = 0;
    adl[87].fbConn = 58;    adl[87].percNote = 0;
    
    // GM89: Pad 1 new age
    adl[88].modChar1 = 119; adl[88].carChar1 = 161;
    adl[88].modChar2 = 165; adl[88].carChar2 = 0;
    adl[88].modChar3 = 83;  adl[88].carChar3 = 160;
    adl[88].modChar4 = 148; adl[88].carChar4 = 5;
    adl[88].modChar5 = 0;   adl[88].carChar5 = 0;
    adl[88].fbConn = 50;    adl[88].percNote = 0;
    
    // GM90: Pad 2 warm
    adl[89].modChar1 = 97;  adl[89].carChar1 = 177;
    adl[89].modChar2 = 31;  adl[89].carChar2 = 128;
    adl[89].modChar3 = 168; adl[89].carChar3 = 37;
    adl[89].modChar4 = 17;  adl[89].carChar4 = 3;
    adl[89].modChar5 = 0;   adl[89].carChar5 = 0;
    adl[89].fbConn = 58;    adl[89].percNote = 0;
    
    // GM91: Pad 3 polysynth
    adl[90].modChar1 = 97;  adl[90].carChar1 = 97;
    adl[90].modChar2 = 23;  adl[90].carChar2 = 0;
    adl[90].modChar3 = 145; adl[90].carChar3 = 85;
    adl[90].modChar4 = 52;  adl[90].carChar4 = 22;
    adl[90].modChar5 = 0;   adl[90].carChar5 = 0;
    adl[90].fbConn = 60;    adl[90].percNote = 0;
    
    // GM92: Pad 4 choir
    adl[91].modChar1 = 113; adl[91].carChar1 = 114;
    adl[91].modChar2 = 93;  adl[91].carChar2 = 0;
    adl[91].modChar3 = 84;  adl[91].carChar3 = 106;
    adl[91].modChar4 = 1;   adl[91].carChar4 = 3;
    adl[91].modChar5 = 0;   adl[91].carChar5 = 0;
    adl[91].fbConn = 48;    adl[91].percNote = 0;
    
    // GM93: Pad 5 bowedpad
    adl[92].modChar1 = 33;  adl[92].carChar1 = 162;
    adl[92].modChar2 = 151; adl[92].carChar2 = 0;
    adl[92].modChar3 = 33;  adl[92].carChar3 = 66;
    adl[92].modChar4 = 67;  adl[92].carChar4 = 53;
    adl[92].modChar5 = 0;   adl[92].carChar5 = 0;
    adl[92].fbConn = 56;    adl[92].percNote = 0;
    
    // GM94: Pad 6 metallic
    adl[93].modChar1 = 161; adl[93].carChar1 = 33;
    adl[93].modChar2 = 28;  adl[93].carChar2 = 0;
    adl[93].modChar3 = 161; adl[93].carChar3 = 49;
    adl[93].modChar4 = 119; adl[93].carChar4 = 71;
    adl[93].modChar5 = 1;   adl[93].carChar5 = 1;
    adl[93].fbConn = 48;    adl[93].percNote = 0;
    
    // GM95: Pad 7 halo
    adl[94].modChar1 = 33;  adl[94].carChar1 = 97;
    adl[94].modChar2 = 137; adl[94].carChar2 = 3;
    adl[94].modChar3 = 17;  adl[94].carChar3 = 66;
    adl[94].modChar4 = 51;  adl[94].carChar4 = 37;
    adl[94].modChar5 = 0;   adl[94].carChar5 = 0;
    adl[94].fbConn = 58;    adl[94].percNote = 0;
    
    // GM96: Pad 8 sweep
    adl[95].modChar1 = 161; adl[95].carChar1 = 33;
    adl[95].modChar2 = 21;  adl[95].carChar2 = 0;
    adl[95].modChar3 = 17;  adl[95].carChar3 = 207;
    adl[95].modChar4 = 71;  adl[95].carChar4 = 7;
    adl[95].modChar5 = 1;   adl[95].carChar5 = 0;
    adl[95].fbConn = 48;    adl[95].percNote = 0;
    
    // GM97: FX 1 rain
    adl[96].modChar1 = 58;  adl[96].carChar1 = 81;
    adl[96].modChar2 = 206; adl[96].carChar2 = 0;
    adl[96].modChar3 = 248; adl[96].carChar3 = 134;
    adl[96].modChar4 = 246; adl[96].carChar4 = 2;
    adl[96].modChar5 = 0;   adl[96].carChar5 = 0;
    adl[96].fbConn = 50;    adl[96].percNote = 0;
    
    // GM98: FX 2 soundtrack
    adl[97].modChar1 = 33;  adl[97].carChar1 = 33;
    adl[97].modChar2 = 21;  adl[97].carChar2 = 0;
    adl[97].modChar3 = 33;  adl[97].carChar3 = 65;
    adl[97].modChar4 = 35;  adl[97].carChar4 = 19;
    adl[97].modChar5 = 1;   adl[97].carChar5 = 0;
    adl[97].fbConn = 48;    adl[97].percNote = 0;
    
    // GM99: FX 3 crystal
    adl[98].modChar1 = 6;   adl[98].carChar1 = 1;
    adl[98].modChar2 = 91;  adl[98].carChar2 = 0;
    adl[98].modChar3 = 116; adl[98].carChar3 = 165;
    adl[98].modChar4 = 149; adl[98].carChar4 = 114;
    adl[98].modChar5 = 0;   adl[98].carChar5 = 0;
    adl[98].fbConn = 48;    adl[98].percNote = 0;
    
    // GM100: FX 4 atmosphere
    adl[99].modChar1 = 34;  adl[99].carChar1 = 97;
    adl[99].modChar2 = 146; adl[99].carChar2 = 131;
    adl[99].modChar3 = 177; adl[99].carChar3 = 242;
    adl[99].modChar4 = 129; adl[99].carChar4 = 38;
    adl[99].modChar5 = 0;   adl[99].carChar5 = 0;
    adl[99].fbConn = 60;    adl[99].percNote = 0;
    
    // GM101: FX 5 brightness
    adl[100].modChar1 = 65;  adl[100].carChar1 = 66;
    adl[100].modChar2 = 77;  adl[100].carChar2 = 0;
    adl[100].modChar3 = 241; adl[100].carChar3 = 242;
    adl[100].modChar4 = 81;  adl[100].carChar4 = 245;
    adl[100].modChar5 = 1;   adl[100].carChar5 = 0;
    adl[100].fbConn = 48;    adl[100].percNote = 0;
    
    // GM102: FX 6 goblins
    adl[101].modChar1 = 97;  adl[101].carChar1 = 163;
    adl[101].modChar2 = 148; adl[101].carChar2 = 128;
    adl[101].modChar3 = 17;  adl[101].carChar3 = 17;
    adl[101].modChar4 = 81;  adl[101].carChar4 = 19;
    adl[101].modChar5 = 1;   adl[101].carChar5 = 0;
    adl[101].fbConn = 54;    adl[101].percNote = 0;
    
    // GM103: FX 7 echoes
    adl[102].modChar1 = 97;  adl[102].carChar1 = 161;
    adl[102].modChar2 = 140; adl[102].carChar2 = 128;
    adl[102].modChar3 = 17;  adl[102].carChar3 = 29;
    adl[102].modChar4 = 49;  adl[102].carChar4 = 3;
    adl[102].modChar5 = 0;   adl[102].carChar5 = 0;
    adl[102].fbConn = 54;    adl[102].percNote = 0;
    
    // GM104: FX 8 sci-fi
    adl[103].modChar1 = 164; adl[103].carChar1 = 97;
    adl[103].modChar2 = 76;  adl[103].carChar2 = 0;
    adl[103].modChar3 = 243; adl[103].carChar3 = 129;
    adl[103].modChar4 = 115; adl[103].carChar4 = 35;
    adl[103].modChar5 = 1;   adl[103].carChar5 = 0;
    adl[103].fbConn = 52;    adl[103].percNote = 0;
    
    // GM105: Sitar
    adl[104].modChar1 = 2;   adl[104].carChar1 = 7;
    adl[104].modChar2 = 133; adl[104].carChar2 = 3;
    adl[104].modChar3 = 210; adl[104].carChar3 = 242;
    adl[104].modChar4 = 83;  adl[104].carChar4 = 246;
    adl[104].modChar5 = 0;   adl[104].carChar5 = 1;
    adl[104].fbConn = 48;    adl[104].percNote = 0;
    
    // GM106: Banjo
    adl[105].modChar1 = 17;  adl[105].carChar1 = 19;
    adl[105].modChar2 = 12;  adl[105].carChar2 = 128;
    adl[105].modChar3 = 163; adl[105].carChar3 = 162;
    adl[105].modChar4 = 17;  adl[105].carChar4 = 229;
    adl[105].modChar5 = 1;   adl[105].carChar5 = 0;
    adl[105].fbConn = 48;    adl[105].percNote = 0;
    
    // GM107: Shamisen
    adl[106].modChar1 = 17;  adl[106].carChar1 = 17;
    adl[106].modChar2 = 6;   adl[106].carChar2 = 0;
    adl[106].modChar3 = 246; adl[106].carChar3 = 242;
    adl[106].modChar4 = 65;  adl[106].carChar4 = 230;
    adl[106].modChar5 = 1;   adl[106].carChar5 = 2;
    adl[106].fbConn = 52;    adl[106].percNote = 0;
    
    // GM108: Koto
    adl[107].modChar1 = 147; adl[107].carChar1 = 145;
    adl[107].modChar2 = 145; adl[107].carChar2 = 0;
    adl[107].modChar3 = 212; adl[107].carChar3 = 235;
    adl[107].modChar4 = 50;  adl[107].carChar4 = 17;
    adl[107].modChar5 = 0;   adl[107].carChar5 = 1;
    adl[107].fbConn = 56;    adl[107].percNote = 0;
    
    // GM109: Kalimba
    adl[108].modChar1 = 4;   adl[108].carChar1 = 1;
    adl[108].modChar2 = 79;  adl[108].carChar2 = 0;
    adl[108].modChar3 = 250; adl[108].carChar3 = 194;
    adl[108].modChar4 = 86;  adl[108].carChar4 = 5;
    adl[108].modChar5 = 0;   adl[108].carChar5 = 0;
    adl[108].fbConn = 60;    adl[108].percNote = 0;
    
    // GM110: Bagpipe
    adl[109].modChar1 = 33;  adl[109].carChar1 = 34;
    adl[109].modChar2 = 73;  adl[109].carChar2 = 0;
    adl[109].modChar3 = 124; adl[109].carChar3 = 111;
    adl[109].modChar4 = 32;  adl[109].carChar4 = 12;
    adl[109].modChar5 = 0;   adl[109].carChar5 = 1;
    adl[109].fbConn = 54;    adl[109].percNote = 0;
    
    // GM111: Fiddle
    adl[110].modChar1 = 49;  adl[110].carChar1 = 33;
    adl[110].modChar2 = 133; adl[110].carChar2 = 0;
    adl[110].modChar3 = 221; adl[110].carChar3 = 86;
    adl[110].modChar4 = 51;  adl[110].carChar4 = 22;
    adl[110].modChar5 = 1;   adl[110].carChar5 = 0;
    adl[110].fbConn = 58;    adl[110].percNote = 0;
    
    // GM112: Shanai
    adl[111].modChar1 = 32;  adl[111].carChar1 = 33;
    adl[111].modChar2 = 4;   adl[111].carChar2 = 129;
    adl[111].modChar3 = 218; adl[111].carChar3 = 143;
    adl[111].modChar4 = 5;   adl[111].carChar4 = 11;
    adl[111].modChar5 = 2;   adl[111].carChar5 = 0;
    adl[111].fbConn = 54;    adl[111].percNote = 0;
    
    // GM113: Tinkle Bell
    adl[112].modChar1 = 5;   adl[112].carChar1 = 3;
    adl[112].modChar2 = 106; adl[112].carChar2 = 128;
    adl[112].modChar3 = 241; adl[112].carChar3 = 195;
    adl[112].modChar4 = 229; adl[112].carChar4 = 229;
    adl[112].modChar5 = 0;   adl[112].carChar5 = 0;
    adl[112].fbConn = 54;    adl[112].percNote = 0;
    
    // GM114: Agogo Bells
    adl[113].modChar1 = 7;   adl[113].carChar1 = 2;
    adl[113].modChar2 = 21;  adl[113].carChar2 = 0;
    adl[113].modChar3 = 236; adl[113].carChar3 = 248;
    adl[113].modChar4 = 38;  adl[113].carChar4 = 22;
    adl[113].modChar5 = 0;   adl[113].carChar5 = 0;
    adl[113].fbConn = 58;    adl[113].percNote = 0;
    
    // GM115: Steel Drums
    adl[114].modChar1 = 5;   adl[114].carChar1 = 1;
    adl[114].modChar2 = 157; adl[114].carChar2 = 0;
    adl[114].modChar3 = 103; adl[114].carChar3 = 223;
    adl[114].modChar4 = 53;  adl[114].carChar4 = 5;
    adl[114].modChar5 = 0;   adl[114].carChar5 = 0;
    adl[114].fbConn = 56;    adl[114].percNote = 0;
    
    // GM116: Woodblock
    adl[115].modChar1 = 24;  adl[115].carChar1 = 18;
    adl[115].modChar2 = 150; adl[115].carChar2 = 0;
    adl[115].modChar3 = 250; adl[115].carChar3 = 248;
    adl[115].modChar4 = 40;  adl[115].carChar4 = 229;
    adl[115].modChar5 = 0;   adl[115].carChar5 = 0;
    adl[115].fbConn = 58;    adl[115].percNote = 0;
    
    // GM117: Taiko Drum
    adl[116].modChar1 = 16;  adl[116].carChar1 = 0;
    adl[116].modChar2 = 134; adl[116].carChar2 = 3;
    adl[116].modChar3 = 168; adl[116].carChar3 = 250;
    adl[116].modChar4 = 7;   adl[116].carChar4 = 3;
    adl[116].modChar5 = 0;   adl[116].carChar5 = 0;
    adl[116].fbConn = 54;    adl[116].percNote = 0;
    
    // GM118: Melodic Tom
    adl[117].modChar1 = 17;  adl[117].carChar1 = 16;
    adl[117].modChar2 = 65;  adl[117].carChar2 = 3;
    adl[117].modChar3 = 248; adl[117].carChar3 = 243;
    adl[117].modChar4 = 71;  adl[117].carChar4 = 3;
    adl[117].modChar5 = 2;   adl[117].carChar5 = 0;
    adl[117].fbConn = 52;    adl[117].percNote = 0;
    
    // GM119: Synth Drum
    adl[118].modChar1 = 1;   adl[118].carChar1 = 16;
    adl[118].modChar2 = 142; adl[118].carChar2 = 0;
    adl[118].modChar3 = 241; adl[118].carChar3 = 243;
    adl[118].modChar4 = 6;   adl[118].carChar4 = 2;
    adl[118].modChar5 = 2;   adl[118].carChar5 = 0;
    adl[118].fbConn = 62;    adl[118].percNote = 0;
    
    // GM120: Reverse Cymbal
    adl[119].modChar1 = 14;  adl[119].carChar1 = 192;
    adl[119].modChar2 = 0;   adl[119].carChar2 = 0;
    adl[119].modChar3 = 31;  adl[119].carChar3 = 31;
    adl[119].modChar4 = 0;   adl[119].carChar4 = 255;
    adl[119].modChar5 = 0;   adl[119].carChar5 = 3;
    adl[119].fbConn = 62;    adl[119].percNote = 0;
    
    // GM121: Guitar FretNoise
    adl[120].modChar1 = 6;   adl[120].carChar1 = 3;
    adl[120].modChar2 = 128; adl[120].carChar2 = 136;
    adl[120].modChar3 = 248; adl[120].carChar3 = 86;
    adl[120].modChar4 = 36;  adl[120].carChar4 = 132;
    adl[120].modChar5 = 0;   adl[120].carChar5 = 2;
    adl[120].fbConn = 62;    adl[120].percNote = 0;
    
    // GM122: Breath Noise
    adl[121].modChar1 = 14;  adl[121].carChar1 = 208;
    adl[121].modChar2 = 0;   adl[121].carChar2 = 5;
    adl[121].modChar3 = 248; adl[121].carChar3 = 52;
    adl[121].modChar4 = 0;   adl[121].carChar4 = 4;
    adl[121].modChar5 = 0;   adl[121].carChar5 = 3;
    adl[121].fbConn = 62;    adl[121].percNote = 0;
    
    // GM123: Seashore
    adl[122].modChar1 = 14;  adl[122].carChar1 = 192;
    adl[122].modChar2 = 0;   adl[122].carChar2 = 0;
    adl[122].modChar3 = 246; adl[122].carChar3 = 31;
    adl[122].modChar4 = 0;   adl[122].carChar4 = 2;
    adl[122].modChar5 = 0;   adl[122].carChar5 = 3;
    adl[122].fbConn = 62;    adl[122].percNote = 0;
    
    // GM124: Bird Tweet
    adl[123].modChar1 = 213; adl[123].carChar1 = 218;
    adl[123].modChar2 = 149; adl[123].carChar2 = 64;
    adl[123].modChar3 = 55;  adl[123].carChar3 = 86;
    adl[123].modChar4 = 163; adl[123].carChar4 = 55;
    adl[123].modChar5 = 0;   adl[123].carChar5 = 0;
    adl[123].fbConn = 48;    adl[123].percNote = 0;
    
    // GM125: Telephone
    adl[124].modChar1 = 53;  adl[124].carChar1 = 20;
    adl[124].modChar2 = 92;  adl[124].carChar2 = 8;
    adl[124].modChar3 = 178; adl[124].carChar3 = 244;
    adl[124].modChar4 = 97;  adl[124].carChar4 = 21;
    adl[124].modChar5 = 2;   adl[124].carChar5 = 0;
    adl[124].fbConn = 58;    adl[124].percNote = 0;
    
    // GM126: Helicopter
    adl[125].modChar1 = 14;  adl[125].carChar1 = 208;
    adl[125].modChar2 = 0;   adl[125].carChar2 = 0;
    adl[125].modChar3 = 246; adl[125].carChar3 = 79;
    adl[125].modChar4 = 0;   adl[125].carChar4 = 245;
    adl[125].modChar5 = 0;   adl[125].carChar5 = 3;
    adl[125].fbConn = 62;    adl[125].percNote = 0;
    
    // GM127: Applause/Noise
    adl[126].modChar1 = 38;  adl[126].carChar1 = 228;
    adl[126].modChar2 = 0;   adl[126].carChar2 = 0;
    adl[126].modChar3 = 255; adl[126].carChar3 = 18;
    adl[126].modChar4 = 1;   adl[126].carChar4 = 22;
    adl[126].modChar5 = 0;   adl[126].carChar5 = 1;
    adl[126].fbConn = 62;    adl[126].percNote = 0;
    
    // GM128: Gunshot
    adl[127].modChar1 = 0;   adl[127].carChar1 = 0;
    adl[127].modChar2 = 0;   adl[127].carChar2 = 0;
    adl[127].modChar3 = 243; adl[127].carChar3 = 246;
    adl[127].modChar4 = 240; adl[127].carChar4 = 201;
    adl[127].modChar5 = 0;   adl[127].carChar5 = 2;
    adl[127].fbConn = 62;    adl[127].percNote = 0;
    
    // GP35: Ac Bass Drum
    adl[128].modChar1 = 16;  adl[128].carChar1 = 17;
    adl[128].modChar2 = 68;  adl[128].carChar2 = 0;
    adl[128].modChar3 = 248; adl[128].carChar3 = 243;
    adl[128].modChar4 = 119; adl[128].carChar4 = 6;
    adl[128].modChar5 = 2;   adl[128].carChar5 = 0;
    adl[128].fbConn = 56;    adl[128].percNote = 35;
    
    // GP36: Bass Drum 1
    adl[129].modChar1 = 16;  adl[129].carChar1 = 17;
    adl[129].modChar2 = 68;  adl[129].carChar2 = 0;
    adl[129].modChar3 = 248; adl[129].carChar3 = 243;
    adl[129].modChar4 = 119; adl[129].carChar4 = 6;
    adl[129].modChar5 = 2;   adl[129].carChar5 = 0;
    adl[129].fbConn = 56;    adl[129].percNote = 35;
    
    // GP37: Side Stick
    adl[130].modChar1 = 2;   adl[130].carChar1 = 17;
    adl[130].modChar2 = 7;   adl[130].carChar2 = 0;
    adl[130].modChar3 = 249; adl[130].carChar3 = 248;
    adl[130].modChar4 = 255; adl[130].carChar4 = 255;
    adl[130].modChar5 = 0;   adl[130].carChar5 = 0;
    adl[130].fbConn = 56;    adl[130].percNote = 52;
    
    // GP38: Acoustic Snare
    adl[131].modChar1 = 0;   adl[131].carChar1 = 0;
    adl[131].modChar2 = 0;   adl[131].carChar2 = 0;
    adl[131].modChar3 = 252; adl[131].carChar3 = 250;
    adl[131].modChar4 = 5;   adl[131].carChar4 = 23;
    adl[131].modChar5 = 2;   adl[131].carChar5 = 0;
    adl[131].fbConn = 62;    adl[131].percNote = 48;
    
    // GP39: Hand Clap
    adl[132].modChar1 = 0;   adl[132].carChar1 = 1;
    adl[132].modChar2 = 2;   adl[132].carChar2 = 0;
    adl[132].modChar3 = 255; adl[132].carChar3 = 255;
    adl[132].modChar4 = 7;   adl[132].carChar4 = 8;
    adl[132].modChar5 = 0;   adl[132].carChar5 = 0;
    adl[132].fbConn = 48;    adl[132].percNote = 58;
    
    // GP40: Electric Snare
    adl[133].modChar1 = 0;   adl[133].carChar1 = 0;
    adl[133].modChar2 = 0;   adl[133].carChar2 = 0;
    adl[133].modChar3 = 252; adl[133].carChar3 = 250;
    adl[133].modChar4 = 5;   adl[133].carChar4 = 23;
    adl[133].modChar5 = 2;   adl[133].carChar5 = 0;
    adl[133].fbConn = 62;    adl[133].percNote = 60;
    
    // GP41: Low Floor Tom
    adl[134].modChar1 = 0;   adl[134].carChar1 = 0;
    adl[134].modChar2 = 0;   adl[134].carChar2 = 0;
    adl[134].modChar3 = 246; adl[134].carChar3 = 246;
    adl[134].modChar4 = 12;  adl[134].carChar4 = 6;
    adl[134].modChar5 = 0;   adl[134].carChar5 = 0;
    adl[134].fbConn = 52;    adl[134].percNote = 47;
    
    // GP42: Closed High Hat
    adl[135].modChar1 = 12;  adl[135].carChar1 = 18;
    adl[135].modChar2 = 0;   adl[135].carChar2 = 0;
    adl[135].modChar3 = 246; adl[135].carChar3 = 251;
    adl[135].modChar4 = 8;   adl[135].carChar4 = 71;
    adl[135].modChar5 = 0;   adl[135].carChar5 = 2;
    adl[135].fbConn = 58;    adl[135].percNote = 43;
    
    // GP43: High Floor Tom
    adl[136].modChar1 = 0;   adl[136].carChar1 = 0;
    adl[136].modChar2 = 0;   adl[136].carChar2 = 0;
    adl[136].modChar3 = 246; adl[136].carChar3 = 246;
    adl[136].modChar4 = 12;  adl[136].carChar4 = 6;
    adl[136].modChar5 = 0;   adl[136].carChar5 = 0;
    adl[136].fbConn = 52;    adl[136].percNote = 49;
    
    // GP44: Pedal High Hat
    adl[137].modChar1 = 12;  adl[137].carChar1 = 18;
    adl[137].modChar2 = 0;   adl[137].carChar2 = 5;
    adl[137].modChar3 = 246; adl[137].carChar3 = 123;
    adl[137].modChar4 = 8;   adl[137].carChar4 = 71;
    adl[137].modChar5 = 0;   adl[137].carChar5 = 2;
    adl[137].fbConn = 58;    adl[137].percNote = 43;
    
    // GP45: Low Tom
    adl[138].modChar1 = 0;   adl[138].carChar1 = 0;
    adl[138].modChar2 = 0;   adl[138].carChar2 = 0;
    adl[138].modChar3 = 246; adl[138].carChar3 = 246;
    adl[138].modChar4 = 12;  adl[138].carChar4 = 6;
    adl[138].modChar5 = 0;   adl[138].carChar5 = 0;
    adl[138].fbConn = 52;    adl[138].percNote = 51;
    
    // GP46: Open High Hat
    adl[139].modChar1 = 12;  adl[139].carChar1 = 18;
    adl[139].modChar2 = 0;   adl[139].carChar2 = 0;
    adl[139].modChar3 = 246; adl[139].carChar3 = 203;
    adl[139].modChar4 = 2;   adl[139].carChar4 = 67;
    adl[139].modChar5 = 0;   adl[139].carChar5 = 2;
    adl[139].fbConn = 58;    adl[139].percNote = 43;
    
    // GP47: Low-Mid Tom
    adl[140].modChar1 = 0;   adl[140].carChar1 = 0;
    adl[140].modChar2 = 0;   adl[140].carChar2 = 0;
    adl[140].modChar3 = 246; adl[140].carChar3 = 246;
    adl[140].modChar4 = 12;  adl[140].carChar4 = 6;
    adl[140].modChar5 = 0;   adl[140].carChar5 = 0;
    adl[140].fbConn = 52;    adl[140].percNote = 54;
    
    // GP48: High-Mid Tom
    adl[141].modChar1 = 0;   adl[141].carChar1 = 0;
    adl[141].modChar2 = 0;   adl[141].carChar2 = 0;
    adl[141].modChar3 = 246; adl[141].carChar3 = 246;
    adl[141].modChar4 = 12;  adl[141].carChar4 = 6;
    adl[141].modChar5 = 0;   adl[141].carChar5 = 0;
    adl[141].fbConn = 52;    adl[141].percNote = 57;
    
    // GP49: Crash Cymbal 1
    adl[142].modChar1 = 14;  adl[142].carChar1 = 208;
    adl[142].modChar2 = 0;   adl[142].carChar2 = 0;
    adl[142].modChar3 = 246; adl[142].carChar3 = 159;
    adl[142].modChar4 = 0;   adl[142].carChar4 = 2;
    adl[142].modChar5 = 0;   adl[142].carChar5 = 3;
    adl[142].fbConn = 62;    adl[142].percNote = 72;
    
    // GP50: High Tom
    adl[143].modChar1 = 0;   adl[143].carChar1 = 0;
    adl[143].modChar2 = 0;   adl[143].carChar2 = 0;
    adl[143].modChar3 = 246; adl[143].carChar3 = 246;
    adl[143].modChar4 = 12;  adl[143].carChar4 = 6;
    adl[143].modChar5 = 0;   adl[143].carChar5 = 0;
    adl[143].fbConn = 52;    adl[143].percNote = 60;
    
    // GP51: Ride Cymbal 1
    adl[144].modChar1 = 14;  adl[144].carChar1 = 7;
    adl[144].modChar2 = 8;   adl[144].carChar2 = 74;
    adl[144].modChar3 = 248; adl[144].carChar3 = 244;
    adl[144].modChar4 = 66;  adl[144].carChar4 = 228;
    adl[144].modChar5 = 0;   adl[144].carChar5 = 3;
    adl[144].fbConn = 62;    adl[144].percNote = 76;
    
    // GP52: Chinese Cymbal
    adl[145].modChar1 = 14;  adl[145].carChar1 = 208;
    adl[145].modChar2 = 0;   adl[145].carChar2 = 10;
    adl[145].modChar3 = 245; adl[145].carChar3 = 159;
    adl[145].modChar4 = 48;  adl[145].carChar4 = 2;
    adl[145].modChar5 = 0;   adl[145].carChar5 = 0;
    adl[145].fbConn = 62;    adl[145].percNote = 84;
    
    // GP53: Ride Bell
    adl[146].modChar1 = 14;  adl[146].carChar1 = 7;
    adl[146].modChar2 = 10;  adl[146].carChar2 = 93;
    adl[146].modChar3 = 228; adl[146].carChar3 = 245;
    adl[146].modChar4 = 228; adl[146].carChar4 = 229;
    adl[146].modChar5 = 3;   adl[146].carChar5 = 1;
    adl[146].fbConn = 54;    adl[146].percNote = 36;
    
    // GP54: Tambourine
    adl[147].modChar1 = 2;   adl[147].carChar1 = 5;
    adl[147].modChar2 = 3;   adl[147].carChar2 = 10;
    adl[147].modChar3 = 180; adl[147].carChar3 = 151;
    adl[147].modChar4 = 4;   adl[147].carChar4 = 247;
    adl[147].modChar5 = 0;   adl[147].carChar5 = 0;
    adl[147].fbConn = 62;    adl[147].percNote = 65;
    
    // GP55: Splash Cymbal
    adl[148].modChar1 = 78;  adl[148].carChar1 = 158;
    adl[148].modChar2 = 0;   adl[148].carChar2 = 0;
    adl[148].modChar3 = 246; adl[148].carChar3 = 159;
    adl[148].modChar4 = 0;   adl[148].carChar4 = 2;
    adl[148].modChar5 = 0;   adl[148].carChar5 = 3;
    adl[148].fbConn = 62;    adl[148].percNote = 84;
    
    // GP56: Cow Bell
    adl[149].modChar1 = 17;  adl[149].carChar1 = 16;
    adl[149].modChar2 = 69;  adl[149].carChar2 = 8;
    adl[149].modChar3 = 248; adl[149].carChar3 = 243;
    adl[149].modChar4 = 55;  adl[149].carChar4 = 5;
    adl[149].modChar5 = 2;   adl[149].carChar5 = 0;
    adl[149].fbConn = 56;    adl[149].percNote = 83;
    
    // GP57: Crash Cymbal 2
    adl[150].modChar1 = 14;  adl[150].carChar1 = 208;
    adl[150].modChar2 = 0;   adl[150].carChar2 = 0;
    adl[150].modChar3 = 246; adl[150].carChar3 = 159;
    adl[150].modChar4 = 0;   adl[150].carChar4 = 2;
    adl[150].modChar5 = 0;   adl[150].carChar5 = 3;
    adl[150].fbConn = 62;    adl[150].percNote = 84;
    
    // GP58: Vibraslap
    adl[151].modChar1 = 128; adl[151].carChar1 = 16;
    adl[151].modChar2 = 0;   adl[151].carChar2 = 13;
    adl[151].modChar3 = 255; adl[151].carChar3 = 255;
    adl[151].modChar4 = 3;   adl[151].carChar4 = 20;
    adl[151].modChar5 = 3;   adl[151].carChar5 = 0;
    adl[151].fbConn = 60;    adl[151].percNote = 24;
    
    // GP59: Ride Cymbal 2
    adl[152].modChar1 = 14;  adl[152].carChar1 = 7;
    adl[152].modChar2 = 8;   adl[152].carChar2 = 74;
    adl[152].modChar3 = 248; adl[152].carChar3 = 244;
    adl[152].modChar4 = 66;  adl[152].carChar4 = 228;
    adl[152].modChar5 = 0;   adl[152].carChar5 = 3;
    adl[152].fbConn = 62;    adl[152].percNote = 77;
    
    // GP60: High Bongo
    adl[153].modChar1 = 6;   adl[153].carChar1 = 2;
    adl[153].modChar2 = 11;  adl[153].carChar2 = 0;
    adl[153].modChar3 = 245; adl[153].carChar3 = 245;
    adl[153].modChar4 = 12;  adl[153].carChar4 = 8;
    adl[153].modChar5 = 0;   adl[153].carChar5 = 0;
    adl[153].fbConn = 54;    adl[153].percNote = 60;
    
    // GP61: Low Bongo
    adl[154].modChar1 = 1;   adl[154].carChar1 = 2;
    adl[154].modChar2 = 0;   adl[154].carChar2 = 0;
    adl[154].modChar3 = 250; adl[154].carChar3 = 200;
    adl[154].modChar4 = 191; adl[154].carChar4 = 151;
    adl[154].modChar5 = 0;   adl[154].carChar5 = 0;
    adl[154].fbConn = 55;    adl[154].percNote = 65;
    
    // GP62: Mute High Conga
    adl[155].modChar1 = 1;   adl[155].carChar1 = 1;
    adl[155].modChar2 = 81;  adl[155].carChar2 = 0;
    adl[155].modChar3 = 250; adl[155].carChar3 = 250;
    adl[155].modChar4 = 135; adl[155].carChar4 = 183;
    adl[155].modChar5 = 0;   adl[155].carChar5 = 0;
    adl[155].fbConn = 54;    adl[155].percNote = 59;

    // GP63: Open High Conga
    adl[156].modChar1 = 1;   adl[156].carChar1 = 2;
    adl[156].modChar2 = 84;  adl[156].carChar2 = 0;
    adl[156].modChar3 = 250; adl[156].carChar3 = 248;
    adl[156].modChar4 = 141; adl[156].carChar4 = 184;
    adl[156].modChar5 = 0;   adl[156].carChar5 = 0;
    adl[156].fbConn = 54;    adl[156].percNote = 51;
    
    // GP64: Low Conga
    adl[157].modChar1 = 1;   adl[157].carChar1 = 2;
    adl[157].modChar2 = 89;  adl[157].carChar2 = 0;
    adl[157].modChar3 = 250; adl[157].carChar3 = 248;
    adl[157].modChar4 = 136; adl[157].carChar4 = 182;
    adl[157].modChar5 = 0;   adl[157].carChar5 = 0;
    adl[157].fbConn = 54;    adl[157].percNote = 45;
    
    // GP65: High Timbale
    adl[158].modChar1 = 1;   adl[158].carChar1 = 0;
    adl[158].modChar2 = 0;   adl[158].carChar2 = 0;
    adl[158].modChar3 = 249; adl[158].carChar3 = 250;
    adl[158].modChar4 = 10;  adl[158].carChar4 = 6;
    adl[158].modChar5 = 3;   adl[158].carChar5 = 0;
    adl[158].fbConn = 62;    adl[158].percNote = 71;

    // GP62: Mute High Conga
    adl[155].modChar1 = 1;   adl[155].carChar1 = 1;
    adl[155].modChar2 = 81;  adl[155].carChar2 = 0;
    adl[155].modChar3 = 250; adl[155].carChar3 = 250;
    adl[155].modChar4 = 135; adl[155].carChar4 = 183;
    adl[155].modChar5 = 0;   adl[155].carChar5 = 0;
    adl[155].fbConn = 54;    adl[155].percNote = 59;
    
    // GP63: Open High Conga
    adl[156].modChar1 = 1;   adl[156].carChar1 = 2;
    adl[156].modChar2 = 84;  adl[156].carChar2 = 0;
    adl[156].modChar3 = 250; adl[156].carChar3 = 248;
    adl[156].modChar4 = 141; adl[156].carChar4 = 184;
    adl[156].modChar5 = 0;   adl[156].carChar5 = 0;
    adl[156].fbConn = 54;    adl[156].percNote = 51;
    
    // GP64: Low Conga
    adl[157].modChar1 = 1;   adl[157].carChar1 = 2;
    adl[157].modChar2 = 89;  adl[157].carChar2 = 0;
    adl[157].modChar3 = 250; adl[157].carChar3 = 248;
    adl[157].modChar4 = 136; adl[157].carChar4 = 182;
    adl[157].modChar5 = 0;   adl[157].carChar5 = 0;
    adl[157].fbConn = 54;    adl[157].percNote = 45;
    
    // GP65: High Timbale
    adl[158].modChar1 = 1;   adl[158].carChar1 = 0;
    adl[158].modChar2 = 0;   adl[158].carChar2 = 0;
    adl[158].modChar3 = 249; adl[158].carChar3 = 250;
    adl[158].modChar4 = 10;  adl[158].carChar4 = 6;
    adl[158].modChar5 = 3;   adl[158].carChar5 = 0;
    adl[158].fbConn = 62;    adl[158].percNote = 71;
    
    // GP66: Low Timbale
    adl[159].modChar1 = 0;   adl[159].carChar1 = 0;
    adl[159].modChar2 = 128; adl[159].carChar2 = 0;
    adl[159].modChar3 = 249; adl[159].carChar3 = 246;
    adl[159].modChar4 = 137; adl[159].carChar4 = 108;
    adl[159].modChar5 = 3;   adl[159].carChar5 = 0;
    adl[159].fbConn = 62;    adl[159].percNote = 60;
    
    // GP67: High Agogo
    adl[160].modChar1 = 3;   adl[160].carChar1 = 12;
    adl[160].modChar2 = 128; adl[160].carChar2 = 8;
    adl[160].modChar3 = 248; adl[160].carChar3 = 246;
    adl[160].modChar4 = 136; adl[160].carChar4 = 182;
    adl[160].modChar5 = 3;   adl[160].carChar5 = 0;
    adl[160].fbConn = 63;    adl[160].percNote = 58;
    
    // GP68: Low Agogo
    adl[161].modChar1 = 3;   adl[161].carChar1 = 12;
    adl[161].modChar2 = 133; adl[161].carChar2 = 0;
    adl[161].modChar3 = 248; adl[161].carChar3 = 246;
    adl[161].modChar4 = 136; adl[161].carChar4 = 182;
    adl[161].modChar5 = 3;   adl[161].carChar5 = 0;
    adl[161].fbConn = 63;    adl[161].percNote = 53;
    
    // GP69: Cabasa
    adl[162].modChar1 = 14;  adl[162].carChar1 = 0;
    adl[162].modChar2 = 64;  adl[162].carChar2 = 8;
    adl[162].modChar3 = 118; adl[162].carChar3 = 119;
    adl[162].modChar4 = 79;  adl[162].carChar4 = 24;
    adl[162].modChar5 = 0;   adl[162].carChar5 = 2;
    adl[162].fbConn = 62;    adl[162].percNote = 64;
    
    // GP70: Maracas
    adl[163].modChar1 = 14;  adl[163].carChar1 = 3;
    adl[163].modChar2 = 64;  adl[163].carChar2 = 0;
    adl[163].modChar3 = 200; adl[163].carChar3 = 155;
    adl[163].modChar4 = 73;  adl[163].carChar4 = 105;
    adl[163].modChar5 = 0;   adl[163].carChar5 = 2;
    adl[163].fbConn = 62;    adl[163].percNote = 71;
    
    // GP71: Short Whistle
    adl[164].modChar1 = 215; adl[164].carChar1 = 199;
    adl[164].modChar2 = 220; adl[164].carChar2 = 0;
    adl[164].modChar3 = 173; adl[164].carChar3 = 141;
    adl[164].modChar4 = 5;   adl[164].carChar4 = 5;
    adl[164].modChar5 = 3;   adl[164].carChar5 = 0;
    adl[164].fbConn = 62;    adl[164].percNote = 61;
    
    // GP72: Long Whistle
    adl[165].modChar1 = 215; adl[165].carChar1 = 199;
    adl[165].modChar2 = 220; adl[165].carChar2 = 0;
    adl[165].modChar3 = 168; adl[165].carChar3 = 136;
    adl[165].modChar4 = 4;   adl[165].carChar4 = 4;
    adl[165].modChar5 = 3;   adl[165].carChar5 = 0;
    adl[165].fbConn = 62;    adl[165].percNote = 61;
    
    // GP73: Short Guiro
    adl[166].modChar1 = 128; adl[166].carChar1 = 17;
    adl[166].modChar2 = 0;   adl[166].carChar2 = 0;
    adl[166].modChar3 = 246; adl[166].carChar3 = 103;
    adl[166].modChar4 = 6;   adl[166].carChar4 = 23;
    adl[166].modChar5 = 3;   adl[166].carChar5 = 3;
    adl[166].fbConn = 62;    adl[166].percNote = 44;
    
    // GP74: Long Guiro
    adl[167].modChar1 = 128; adl[167].carChar1 = 17;
    adl[167].modChar2 = 0;   adl[167].carChar2 = 9;
    adl[167].modChar3 = 245; adl[167].carChar3 = 70;
    adl[167].modChar4 = 5;   adl[167].carChar4 = 22;
    adl[167].modChar5 = 2;   adl[167].carChar5 = 3;
    adl[167].fbConn = 62;    adl[167].percNote = 40;
    
    // GP75: Claves
    adl[168].modChar1 = 6;   adl[168].carChar1 = 21;
    adl[168].modChar2 = 63;  adl[168].carChar2 = 0;
    adl[168].modChar3 = 0;   adl[168].carChar3 = 247;
    adl[168].modChar4 = 244; adl[168].carChar4 = 245;
    adl[168].modChar5 = 0;   adl[168].carChar5 = 0;
    adl[168].fbConn = 49;    adl[168].percNote = 69;
    
    // GP76: High Wood Block
    adl[169].modChar1 = 6;   adl[169].carChar1 = 18;
    adl[169].modChar2 = 63;  adl[169].carChar2 = 0;
    adl[169].modChar3 = 0;   adl[169].carChar3 = 247;
    adl[169].modChar4 = 244; adl[169].carChar4 = 245;
    adl[169].modChar5 = 3;   adl[169].carChar5 = 0;
    adl[169].fbConn = 48;    adl[169].percNote = 68;
    
    // GP77: Low Wood Block
    adl[170].modChar1 = 6;   adl[170].carChar1 = 18;
    adl[170].modChar2 = 63;  adl[170].carChar2 = 0;
    adl[170].modChar3 = 0;   adl[170].carChar3 = 247;
    adl[170].modChar4 = 244; adl[170].carChar4 = 245;
    adl[170].modChar5 = 0;   adl[170].carChar5 = 0;
    adl[170].fbConn = 49;    adl[170].percNote = 63;
    
    // GP78: Mute Cuica
    adl[171].modChar1 = 1;   adl[171].carChar1 = 2;
    adl[171].modChar2 = 88;  adl[171].carChar2 = 0;
    adl[171].modChar3 = 103; adl[171].carChar3 = 117;
    adl[171].modChar4 = 231; adl[171].carChar4 = 7;
    adl[171].modChar5 = 0;   adl[171].carChar5 = 0;
    adl[171].fbConn = 48;    adl[171].percNote = 74;
    
    // GP79: Open Cuica
    adl[172].modChar1 = 65;  adl[172].carChar1 = 66;
    adl[172].modChar2 = 69;  adl[172].carChar2 = 8;
    adl[172].modChar3 = 248; adl[172].carChar3 = 117;
    adl[172].modChar4 = 72;  adl[172].carChar4 = 5;
    adl[172].modChar5 = 0;   adl[172].carChar5 = 0;
    adl[172].fbConn = 48;    adl[172].percNote = 60;
    
    // GP80: Mute Triangle
    adl[173].modChar1 = 10;  adl[173].carChar1 = 30;
    adl[173].modChar2 = 64;  adl[173].carChar2 = 78;
    adl[173].modChar3 = 224; adl[173].carChar3 = 255;
    adl[173].modChar4 = 240; adl[173].carChar4 = 5;
    adl[173].modChar5 = 3;   adl[173].carChar5 = 0;
    adl[173].fbConn = 56;    adl[173].percNote = 80;
    
    // GP81: Open Triangle
    adl[174].modChar1 = 10;  adl[174].carChar1 = 30;
    adl[174].modChar2 = 124; adl[174].carChar2 = 82;
    adl[174].modChar3 = 224; adl[174].carChar3 = 255;
    adl[174].modChar4 = 240; adl[174].carChar4 = 2;
    adl[174].modChar5 = 3;   adl[174].carChar5 = 0;
    adl[174].fbConn = 56;    adl[174].percNote = 64;
    
    // GP82
    adl[175].modChar1 = 14;  adl[175].carChar1 = 0;
    adl[175].modChar2 = 64;  adl[175].carChar2 = 8;
    adl[175].modChar3 = 122; adl[175].carChar3 = 123;
    adl[175].modChar4 = 74;  adl[175].carChar4 = 27;
    adl[175].modChar5 = 0;   adl[175].carChar5 = 2;
    adl[175].fbConn = 62;    adl[175].percNote = 72;
    
    // GP83
    adl[176].modChar1 = 14;  adl[176].carChar1 = 7;
    adl[176].modChar2 = 10;  adl[176].carChar2 = 64;
    adl[176].modChar3 = 228; adl[176].carChar3 = 85;
    adl[176].modChar4 = 228; adl[176].carChar4 = 57;
    adl[176].modChar5 = 3;   adl[176].carChar5 = 1;
    adl[176].fbConn = 54;    adl[176].percNote = 73;
    
    // GP84
    adl[177].modChar1 = 5;   adl[177].carChar1 = 4;
    adl[177].modChar2 = 5;   adl[177].carChar2 = 64;
    adl[177].modChar3 = 249; adl[177].carChar3 = 214;
    adl[177].modChar4 = 50;  adl[177].carChar4 = 165;
    adl[177].modChar5 = 3;   adl[177].carChar5 = 0;
    adl[177].fbConn = 62;    adl[177].percNote = 70;
    
    // GP85
    adl[178].modChar1 = 2;   adl[178].carChar1 = 21;
    adl[178].modChar2 = 63;  adl[178].carChar2 = 0;
    adl[178].modChar3 = 0;   adl[178].carChar3 = 247;
    adl[178].modChar4 = 243; adl[178].carChar4 = 245;
    adl[178].modChar5 = 3;   adl[178].carChar5 = 0;
    adl[178].fbConn = 56;    adl[178].percNote = 68;
    
    // GP86
    adl[179].modChar1 = 1;   adl[179].carChar1 = 2;
    adl[179].modChar2 = 79;  adl[179].carChar2 = 0;
    adl[179].modChar3 = 250; adl[179].carChar3 = 248;
    adl[179].modChar4 = 141; adl[179].carChar4 = 181;
    adl[179].modChar5 = 0;   adl[179].carChar5 = 0;
    adl[179].fbConn = 55;    adl[179].percNote = 48;
    
    // GP87
    adl[180].modChar1 = 0;   adl[180].carChar1 = 0;
    adl[180].modChar2 = 0;   adl[180].carChar2 = 0;
    adl[180].modChar3 = 246; adl[180].carChar3 = 246;
    adl[180].modChar4 = 12;  adl[180].carChar4 = 6;
    adl[180].modChar5 = 0;   adl[180].carChar5 = 0;
    adl[180].fbConn = 52;    adl[180].percNote = 53;
}
