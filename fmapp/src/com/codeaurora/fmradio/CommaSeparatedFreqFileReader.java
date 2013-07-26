package com.codeaurora.fmradio;

import android.util.Log;

import java.io.*;

public class CommaSeparatedFreqFileReader implements GetNextFreqInterface {

   private BufferedReader reader;
   private String fileName;
   private String [] freqList;
   private int index;
   private static final String LOGTAG = "COMMA_SEPARATED_FREQ_PARSER";
   private boolean errorHasOcurred;

   @Override
   public int getNextFreq() {
      int freq = Integer.MAX_VALUE;

      Log.d(LOGTAG, "Inside function get freq");
      if(!errorHasOcurred) {
         if(index < freqList.length) {
            try {
                freq = (int)(Float.parseFloat(freqList[index]) * 1000);
            }catch(NumberFormatException e) {
                Log.d(LOGTAG, "Format exception");
            }
            index++;
            if(index >= freqList.length) {
               index = 0;
               readLineAndParse();
            }
            return freq;
         }else {
            return Integer.MAX_VALUE;
         }
      }else {
         return Integer.MAX_VALUE;
      }
   }

   public CommaSeparatedFreqFileReader(String fileName) {
      this.fileName = fileName;
      try {
           reader =  new BufferedReader(new FileReader(this.fileName));
           readLineAndParse();
      }catch(Exception e) {
           errorHasOcurred = true;
           Log.d(LOGTAG, "File not found");
      }
   }

   private void readLineAndParse() {
      String curLine;
      if(reader != null) {
         try {
              if((curLine = reader.readLine()) != null) {
                 freqList = curLine.split(",");
              }else {
                 reader.close();
                 reader = null;
                 errorHasOcurred = true;
              }
         }catch(Exception e) {
              errorHasOcurred = true;
         }
      }else {
         errorHasOcurred = true;
      }
   }

   @Override
   public void Stop() {
      if(reader != null) {
         try {
             reader.close();
             reader = null;
         }catch(Exception e) {
         }
         errorHasOcurred = true;
      }
   }

   @Override
   public boolean errorOccured() {
      return errorHasOcurred;
   }
}

