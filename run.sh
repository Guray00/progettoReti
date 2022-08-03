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

# gnome-terminal -- ./server 4242
gnome-terminal -x sh -c "./serv 4242;printf \"\nProcesso terminato, verrà chiuso tra 5 secondi...\"; sleep 5;"
gnome-terminal -x sh -c "./dev 1234; printf \"\nProcesso terminato, verrà chiuso tra 5 secondi...\"; sleep 5;"
#gnome;-terminal -- ./dev 1234
