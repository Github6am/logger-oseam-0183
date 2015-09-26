/* nmea_cksum2.c
 * 
 * Purpose:
 *     Tool to check OpenSeaMapLogger files and restore checksums
 * 
 * Make:
       gcc -g -o nmea_cksum2 nmea_cksum2.c ; cp nmea_cksum2 ~/bin
 *
 * Usage examples:
       echo ABC0D | ./nmea_cksum2
       echo '$*00' | ./nmea_cksum2
       echo '$*46' | ./nmea_cksum2
       cat DATA0001.DAT | ./nmea_cksum2 | more 
       cat DATA0001.DAT | ./nmea_cksum2 | grep -v "#"  
       echo '00:00:34.597;I;$POSMACC,16644,-200,2024*46' | nmea_cksum2
       echo '00:00:34.597;I;$POSMACC,16644,-200,2024'    | nmea_cksum2 -a
 *
 *
 * Background:
 *   - derived from binlog2hexlog.c
 *   - http://wkla.no-ip.biz/ArduinoWiki/doku.php?id=arduino:oseam
     - Seatalk data is contained in messages like $POSMSK,0042607000*32
       and needs to be converted to NMEA with posmsk2nmea.sh
     - git repositories
       db6am   https://github.com/Github6am/logger-oseam-0183.git
       origin  https://github.com/willie68/OpenSeaMapLogger
       oseam   https://github.com/OpenSeaMap/logger-oseam-0183.git
 *
 *
 *   - typical data to be parsed:
 *     00:00:34.597;I;$POSMACC,16644,-200,2024*46
 *     00:00:34.596;I;$POSMVCC,5143,4943*5E
 *     00:00:34.598;B;$GPVTG,,T,247.3,M,0.0,N
 *     00:00:35.406;B;$GPRMC,123158,A,4309.9431,N,01348.5853,E,0.0,250.4,250814,3,E*6A
 *
 *   - simple state machine implementation:
 *            00:00:34.596;I;$POSMVCC,5143,4943*5E
 *     state: 1  2  3  4   5  6                 7  8
 *
 *  Author: 
 *     Andreas Merz, 2015
 *
 *  Licence:
 *     GPL
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

char help[]="\n\
  nmea_cksum2 [-a] [-h] [-d debuglevel] [-s separator]\n\
  \n\
  validate or append [-a] NMEA checksum.\n\
  Invalid sentences are marked with '#' signs \n\
  \n\
  Usage Examples: \n\
    cat DATA0001.DAT | nmea_cksum2 | grep -v '#'  # filter invalid data\n\
    cat DATA0001.DAT | nmea_cksum2 -a             # append missing checksums\n\
  \n\
  Version: 1.0\
  \n\n";

unsigned int a2nibble(unsigned char nibble)
{
  nibble=nibble | 0x20;  // make lowercase
  if (nibble > '9') 
    return( nibble - 'a'+10);
  return(nibble-'0');
}

// calculate checksum over a string buffer
void update_cksum(unsigned int *csum, unsigned char *str)
{ 
  while(*str){
    *csum ^= *(str++);
  }
} 


main(int argc, char *argv[]) 
{
   int opt;
   unsigned int c1,c0='\n';   // previous and actual character
   unsigned int state=0;
   unsigned int col=0;
   unsigned int csum=0;
   unsigned int optdebug=0; 
   unsigned int opta=0;       // append checksum
   unsigned char *sep0=",";   // separate binary message header from body
   unsigned char *sep1="";    // default: point to empty string
   unsigned char buf[16];     // buffer for checksum calculation, no boundary check!
   
   // parse command line arguments
   while ((opt = getopt(argc, argv, "hac:d:s:")) != -1) {
       switch (opt) {
       case 's':
           sep1 = optarg;
           sep0 = ""; 
           break;
       case 'd':
           optdebug = atoi(optarg);
           break;
       case 'a':
           opta = 1;
           break;
       case 'c':
           printf("code 0x%02X %s %c\n", *optarg, optarg, atoi(optarg));  // undocumented character conversion
           return;
           break;
       case 'h':
           printf( "%s",help);
           return 0;
           break;
       default:
           fprintf(stderr, "\nvalidate checksum in OpenSeaMapLogger files containing nmea data\n");
           fprintf(stderr, "Usage: cat DATA0001.DAT | %s [-h] [-a] [-d debuglevel] [-s separator]\n", argv[0]);
           exit(EXIT_FAILURE);
       }
   }
   
   // now process stdinput
   while(!feof(stdin)) {
     c1=c0;
     c0=getchar();
     switch(state) {

       case 0:                          // sync state
         if(isdigit(c0) && ((c1=='\n') || (c1=='\r'))) {
           state++;
         }
         else if( (c0=='$' || c0=='!') && ((c1=='\n') || (c1=='\r')) ) {
           state=7;
         }
         break;
          

       case 1:                          // timestamp H state
         if(isdigit(c1) && c0==':') {
           state++;
         }
         else if( isdigit(c0) || ((c1=='\n') || (c1=='\r'))) {
           // stay
         }
         else {
           state=0;
         }
         break;


       case 2:                          // timestamp M state
         if(isdigit(c1) && c0==':') {
           state++;
         }
         else if( isdigit(c0)) {
           // stay
         }
         else {
           state=0;
         }
         break;


       case 3:                          // timestamp S state
         if(isdigit(c1) && c0=='.') {
           state++;
         }
         else if( isdigit(c0)) {
           // stay
         }
         else {
           state=0;
         }
         break;

       case 4:                          // timestamp ms state
         if(isdigit(c1) && c0==';') {
           state++;
         }
         else if( isdigit(c0)) {
           // stay
         }
         else {
           state=0;
         }
         break;


       case 5:                          // channel
         if ( (c0 >='A') && (c0 <='Z')) {
           //stay
         }
         else if((c0 ==';')) {
           state++;
         }
         else {
           state=0;
         }
         break;
         

       case 6:                          // NMEA start
         if((c0=='$') || (c0=='!') ) {
           csum = 0;
           state++;
         }
         else {
           state=0;
           printf("#nodollar#");    // mark unexpected character
           csum=0;
         }
         break;


       case 7:                          // NMEA or AIS messge body
         if(c0=='*') {
           state++;
         }
         else if((c0=='\n') || (c0=='\r')) {
           if(opta) {
             printf("*%02X",csum);  // append a new checksum
           }
           else {
             printf("##");  // mark unexpected line ending
           }
           state=1;       
         }
         else if( c0 < 0x20 || (c0=='$')) {    // likely corrupted
           printf("###");          
           state=0;
         }
         else {
           // stay
           csum^=c0;
         }
         break;


       case 8:                          // NMEA checksum
         if((c0=='\n') || (c0=='\r')) {
           if(csum) 
             printf(" # checksum error, residual: %02X", csum>>8);
           state=1;
         }
         else {
           // stay
           csum ^= a2nibble(c0)<<4;
           csum <<= 4;
         }
         break;


       default:
         printf(" # %02x %02x ",c1, c0);
     
     }
     
     if( (state > 0) && (state < 10) && (c0>=0x0A) && (c0<0x80))
       printf("%c",c0);

     
     if(optdebug>0) {
       if(c0>=0x20)
         printf("  state: %2d   %c  %c      %02x\n", state, c1, c0, (csum&0xFF));
       else
         printf("  state: %2d  %02x %02x\n", state, c1, c0);
     }
   }
   
   //printf("\n");

}
