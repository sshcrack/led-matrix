#!/bin/bash
rsync -avz --delete build/install/ ledmat:/home/pi/ledmat/run/

ssh ledmat sudo service ledmat restart