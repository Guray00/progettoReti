#!/bin/bash

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
# Yaxis=$(xrandr --current | grep '*' | uniq | awk '{print $1}' | cut -d 'x' -f2)
# 
# borderX="$((Xaxis/20))"
# borderY="$((Yaxis/10))"
# 
# Xlog="$((Xaxis-borderX-200))"
# Ylog="$((0+borderY))"

# gnome-terminal -- ./server 4242
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


gnome-terminal --geometry 100x50+2000+100  -x sh -c "tail -F .log"
gnome-terminal --geometry 100x30+50+100    -x sh -c "./serv 4242; printf \"\nProcesso terminato, verrà chiuso tra 5 secondi...\"; sleep 5;"
gnome-terminal --geometry 100x30+50+800    -x sh -c "./dev 1234; printf \"\nProcesso terminato, verrà chiuso tra 5 secondi...\"; sleep 5;"
# gnome-terminal --geometry 100x30+1050+800    -x sh -c "./dev 1235; printf \"\nProcesso terminato, verrà chiuso tra 5 secondi...\"; sleep 5;"

#gnome;-terminal -- ./dev 1234
