package org.filemq;

import static org.junit.Assert.*;

import org.junit.Test;

public class TestFmqConfig
{
    @Test
    public void testConfig () {
        System.out.printf (" * fmq_config: ");
    
        //  @selftest
        //  We create a config of this structure:
        //
        //  root
        //      type = zqueue
        //      frontend
        //          option
        //              swap = 25000000     #  25MB
        //              subscribe = #2
        //              hwm = 1000
        //          bind = tcp://*:5555
        //      backend
        //          bind = tcp://*:5556
        //
        FmqConfig
            root,
            type,
            frontend,
            option,
            hwm,
            swap,
            subscribe,
            bind,
            backend;
    
        //  Left is first child, next is next sibling
        root     = new FmqConfig ("root", null);
        type     = new FmqConfig ("type", root);
        type.setValue ("zqueue");
        frontend = new FmqConfig ("frontend", root);
        option   = new FmqConfig ("option", frontend);
        swap     = new FmqConfig ("swap", option);
        swap.setValue ("25000000");
        subscribe = new FmqConfig ("subscribe", option);
        subscribe.formatValue ("#%d", 2);
        hwm      = new FmqConfig ("hwm", option);
        hwm.setValue ("1000");
        bind     = new FmqConfig ("bind", frontend);
        bind.setValue ("tcp://*:5555");
        backend  = new FmqConfig ("backend", root);
        bind     = new FmqConfig ("bind", backend);
        bind.setValue ("tcp://*:5556");
    
        assertEquals (1000, Integer.parseInt (root.resolve ("frontend/option/hwm", "0")));
    }
}
