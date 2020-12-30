/*
 * This file is part of libbluray
 * Copyright (C) 2020  Petri Hintukainen <phintuka@users.sourceforge.net>
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

package org.videolan.patchers;

/**
 * Replace method calls in Xlet
 */

import java.util.Map;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Attribute;

import org.videolan.BDJClassFilePatcher;
import org.videolan.Logger;

public class ReplaceMethodPatcher implements BDJClassFilePatcher
{
    /* replace static method calls */

    final String origMethod, origClass, newMethod, newClass, signature;
    final int callOpcode;

    public ReplaceMethodPatcher(String origClass, String origMethod, String newClass, String newMethod, String signature) {

        if (origMethod == null || origClass == null || newMethod == null || newClass == null || signature == null)
            throw new NullPointerException();

        this.origMethod = origMethod;
        this.origClass = origClass;
        this.newMethod = newMethod;
        this.newClass = newClass;
        this.signature = signature;
        this.callOpcode = Opcodes.INVOKESTATIC;
    }

    /*
     * Replace method call
     */

    public byte[] patch(byte[] b) throws ClassFormatError {
        try {
            ClassReader cr = new ClassReader(b);
            ClassWriter cw = new ClassWriter(cr, 0/*ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS*/);
            ClassVisitor cv = new PatchClassVisitor(cw);
            cr.accept(cv, ClassReader.SKIP_DEBUG | ClassReader.EXPAND_FRAMES);
            return cw.toByteArray();
        } catch (Exception e) {
            logger.error("Failed patching class: " + e);
        }

        return b;
    }

    private final class PatchClassVisitor extends ClassVisitor {

        public PatchClassVisitor(ClassVisitor cv) {
            super(Opcodes.ASM4, cv);
        }

        public MethodVisitor visitMethod(int access, String name, String desc,
                                         String signature, String[] exceptions) {
            MethodVisitor mv = super.visitMethod(access, name, desc, signature, exceptions);
            return new PatchMethodVisitor(mv);
        }
    }

    private final class PatchMethodVisitor extends MethodVisitor {
        public PatchMethodVisitor(MethodVisitor mv) {
            super(Opcodes.ASM4, mv);
        }

        public void visitAttribute(Attribute attr) {
            super.visitAttribute(attr);
        }

        public void visitMethodInsn(int opcode, String owner, String name, String desc, boolean itf) {
                if (opcode == callOpcode &&
                    owner.equals(origClass) &&
                    name.equals(origMethod) &&
                    desc.equals(signature)) {

                    // replace funtion call
                    super.visitMethodInsn(callOpcode, newClass, newMethod, signature, false);
                }
                else {
                    // no change
                    super.visitMethodInsn(opcode, owner, name, desc, itf);
                }
        }
    }

    private static final Logger logger = Logger.getLogger(ReplaceMethodPatcher.class.getName());
}
