import { useState } from 'react';
import './CameraViewer.css';

interface CameraViewerProps {
  url?: string;
}

const VIDEO_EXTENSIONS = ['.mp4', '.webm', '.ogg'];

// Observação: RTSP não roda diretamente no navegador; uma câmera RTSP
// precisaria ser convertida pelo backend (ex.: para HLS ou MJPEG) antes de
// chegar até aqui.

export function CameraViewer({ url }: CameraViewerProps) {
  // Guarda qual URL falhou: se a URL mudar, o componente tenta exibi-la de novo.
  const [failedUrl, setFailedUrl] = useState<string | null>(null);

  const showFallback = !url || failedUrl === url;
  // Ignora query string (ex.: stream.m3u8?token=x) ao detectar o formato.
  const path = (url ?? '').split('?')[0].toLowerCase();
  const isVideo = path.endsWith('.m3u8') || VIDEO_EXTENSIONS.some((ext) => path.endsWith(ext));

  return (
    <div className="camera-viewer">
      <h2 className="camera-viewer-title">Câmera do sistema físico</h2>
      <div className="camera-viewer-frame">
        {showFallback && (
          <div className="camera-viewer-fallback">
            <span>Câmera indisponível</span>
          </div>
        )}

        {!showFallback && isVideo && (
          // Vídeo HTTP ou HLS (.m3u8). HLS nativo funciona em Safari; em
          // Chrome/Firefox, uma biblioteca como hls.js pode ser adicionada
          // futuramente — não instalada nesta versão.
          <video
            className="camera-viewer-media"
            src={url}
            autoPlay
            muted
            loop
            playsInline
            onError={() => setFailedUrl(url)}
          />
        )}

        {!showFallback && !isVideo && (
          // Qualquer outra URL é tratada como MJPEG.
          <img
            className="camera-viewer-media"
            src={url}
            alt="Câmera do sistema físico"
            onError={() => setFailedUrl(url)}
          />
        )}
      </div>
    </div>
  );
}
