#!/bin/bash

# Nota: Questo file avvia l'applicativo senza però compilarlo!


# ======================================================================
# ATTENZIONE: il flag debug riportato di seguito consente l'apertura
# della shell aggiuntiva per avere in real-time una finestra di debug
# e osservare cosa l'applicativo sta eseguendo.
DEBUG=false
# ======================================================================

end () {
  echo "5..."
  sleep 1
  echo "4..."
  sleep 1
  echo "3..."
  sleep 1
  echo "2..."
  sleep 1
  echo "1..."
  sleep 1
}

Xaxis=$(xrandr --current | grep '*' | uniq | awk '{print $1}' | cut -d 'x' -f1)

> .log
cat <<EOT >> .log
****************************************************************************************************
                          ██████╗ ███████╗██████╗ ██╗   ██╗ ██████╗ 
                          ██╔══██╗██╔════╝██╔══██╗██║   ██║██╔════╝ 
                          ██║  ██║█████╗  ██████╔╝██║   ██║██║  ███╗
                          ██║  ██║██╔══╝  ██╔══██╗██║   ██║██║   ██║
                          ██████╔╝███████╗██████╔╝╚██████╔╝╚██████╔╝
                          ╚═════╝ ╚══════╝╚═════╝  ╚═════╝  ╚═════╝ 
****************************************************************************************************

EOT

if [ "$DEBUG" = true ] ; then
  gnome-terminal --geometry 100x50+2000+100  -x sh -c "tail -F .log"
fi
gnome-terminal --geometry 100x30+50+100    -x sh -c "./serv 4242; printf \"\nProcesso terminato, verrà chiuso tra 5 secondi...\"; sleep 5;"
gnome-terminal --geometry 100x30+1050+100  -x sh -c "./dev 1234; printf \"\nProcesso terminato, verrà chiuso tra 5 secondi...\"; sleep 5;"
gnome-terminal --geometry 100x30+50+800    -x sh -c "./dev 1235; printf \"\nProcesso terminato, verrà chiuso tra 5 secondi...\"; sleep 5;"
gnome-terminal --geometry 100x30+1050+800  -x sh -c "./dev 1236; printf \"\nProcesso terminato, verrà chiuso tra 5 secondi...\"; sleep 5;"
