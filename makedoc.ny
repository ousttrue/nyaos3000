echo create documents...
foreach i nyados nyacus nyaos2
    ruby -rerb -e "opt='"%i%"';ERB.new(STDIN.read,nil,1).run" < document.erb > %i%.txt
    echo %i%.txt created.
end
