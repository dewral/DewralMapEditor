import { defineConfig } from 'vite';
export default defineConfig({root:'web',base:'./',build:{outDir:'../dist/public',emptyOutDir:true},server:{proxy:{'/api':'http://localhost:3000'}}});
