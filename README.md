ffss - fabulous fabulous snapshots
===================================

ffss is a CLI tool for taking high quality screenshots from video files.

Depends: ffmpegsource, libpng++, boost

To build, just run 
    ``make``

usage: ``ffss [options] inputfile``

Examples: To get frames 100, 200, 300 from input.mkv
    ``ffss -f 100 -f 200 -f 300 input.mkv``
The output snapshots are saved as
    ``snapshot_${FRAMENUM}${FRAMETYPE}.png``
where "snapshot" is the default prefix.
You can specify the prefix with ``--output`` or ``-o``
If you wanted to save the index for later
    ``ffss --index input.ffvideoindex -f 100 input.mkv``
Running the previous command again will re-use the index, which will save
time with large videos.

It is also possible to specify the input file with ``--input`` or ``-i`` if
you desire.