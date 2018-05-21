/*
 * This file is part of libbluray
 * Copyright (C) 2015  Petri Hintukainen <phintuka@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

package org.videolan;

/**
 * This is a class which is called by BDJClassLoader
 * when ClassFormatError is thrown inside defineClass().
 *
 * Some discs have invalid debug info in class files (broken by
 * malfunctioning obfuscater ?).
 * We strip debug info from the class and try to load it again.
 *
 * Penguins of MAdagascar:
 *   java.lang.ClassFormatError: Invalid index 0 in LocalVariableTable'
 *       in class file com/tcs/blr/bluray/pal/fox/controller/d
 */

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Attribute;

class BDJClassFileTransformer
{
    public byte[] transform(byte[] b, int off, int len)
        throws ClassFormatError
    {
        logger.info("Trying to transform broken class file (" + len + " bytes)");

        byte[] r = new byte[len];
        for (int i = 0; i < len; i++)
            r[i] = b[i+off];

        try {
            ClassReader cr = new ClassReader(r);
            ClassWriter cw = new ClassWriter(cr, 0/*ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS*/);
            ClassVisitor cv = new MyClassVisitor(cw);
            cr.accept(cv, ClassReader.SKIP_DEBUG);
            return cw.toByteArray();
        } catch (Exception e) {
            logger.error("Failed transforming class: " + e);
        }

        return r;
    }

    public class MyClassVisitor extends ClassVisitor {
        public MyClassVisitor(ClassVisitor cv) {
            super(Opcodes.ASM4, cv);
        }

        public MethodVisitor visitMethod(int access, String name, String desc,
                                         String signature, String[] exceptions) {
            MethodVisitor mv = super.visitMethod(access, name, desc, signature, exceptions);
            //System.err.println("visit method: " + name);
            return new MyMethodVisitor(mv);
        }
    }

    public class MyMethodVisitor extends MethodVisitor {
        public MyMethodVisitor(MethodVisitor mv) {
            super(Opcodes.ASM4, mv);
        }

        public void visitAttribute(Attribute attr) {
            //System.err.println("    attribute: " + attr.type);
            super.visitAttribute(attr);
        }
    }

    private static final Logger logger = Logger.getLogger(BDJClassFileTransformer.class.getName());
}
