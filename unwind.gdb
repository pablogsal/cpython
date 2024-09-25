define unwind_stack_fp
    set $xbp = (void **)$arg0
    while 1
      x/2a $xbp
      set $xbp = (void **)$xbp[0]
    end
    #set $fp = $rbp
    #set $counter = 0
    #while $fp != 0
    #    set $rip = *(void **)($fp + 8)
    #    set $next_fp = *(void **)$fp
    #    #printf "#%d  0x%lx in ", $counter, (unsigned long)$rip
    #    #info symbol $rip
    #    set $fp = $next_fp
    #    set $counter = $counter + 1
    #    if $counter > 100
    #        printf "Reached maximum depth. Stopping.\n"
    #        loop_break
    #    end
    #end
end
